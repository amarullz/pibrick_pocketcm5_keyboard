#!/bin/bash

VID="F10C"
PID="0001"
INTERFACE="01"

REPORT_SIZE=32

PIBRICK_CMD=255

CMD_TIMEOUT=1
CMD_BACKLIGHT=2
CMD_RGB=3

GET=0
SET=1

STATUS_OK=0
STATUS_ERROR=1

QUIET=false


# ------------------------------------------------------------
# Find piBrick Raw HID interface
# ------------------------------------------------------------

find_hidraw() {
    for hid in /sys/bus/hid/devices/0003:${VID}:${PID}.*; do
        [ -d "$hid" ] || continue

        local interface

        interface=$(
            udevadm info -a -p "$hid" 2>/dev/null |
            grep -m1 'ATTRS{bInterfaceNumber}' |
            sed -E 's/.*=="([^"]+)".*/\1/'
        )

        [ "$interface" = "$INTERFACE" ] || continue

        for raw in "$hid"/hidraw/hidraw*; do
            [ -e "$raw" ] || continue

            echo "/dev/$(basename "$raw")"
            return 0
        done
    done

    return 1
}


# ------------------------------------------------------------
# Send 32-byte Raw HID report
# ------------------------------------------------------------

send_report() {
    local hidraw="$1"
    shift

    local report=""
    local byte

    for byte in "$@"; do
        printf -v byte '\\x%02x' "$byte"
        report+="$byte"
    done

    printf '%b' "$report" |
        dd of="$hidraw" \
           bs="$REPORT_SIZE" \
           conv=sync \
           status=none
}


# ------------------------------------------------------------
# Send command and wait for response
# ------------------------------------------------------------

send_and_read() {
    local hidraw="$1"
    shift

    RESPONSE_FILE=$(mktemp)

    (
        timeout 2 dd \
            if="$hidraw" \
            of="$RESPONSE_FILE" \
            bs="$REPORT_SIZE" \
            count=1 \
            2>/dev/null
    ) &

    local reader_pid=$!

    sleep 0.05

    send_report "$hidraw" "$@"

    wait "$reader_pid"

    if [ ! -s "$RESPONSE_FILE" ]; then
        rm -f "$RESPONSE_FILE"
        RESPONSE_FILE=""
        echo "Error: no response received." >&2
        return 1
    fi

    return 0
}


# ------------------------------------------------------------
# Read one byte from response
# ------------------------------------------------------------

response_byte() {
    local offset="$1"

    od -An -tu1 \
        -j "$offset" \
        -N 1 \
        "$RESPONSE_FILE" |
        tr -d ' '
}


# ------------------------------------------------------------
# Validate firmware response
#
# Response:
#
#   byte 0 = PIBRICK_CMD
#   byte 1 = command
#   byte 2 = status
#   byte 3 = value
# ------------------------------------------------------------

check_response() {
    local expected_cmd="$1"

    local marker
    local command
    local status

    marker=$(response_byte 0)
    command=$(response_byte 1)
    status=$(response_byte 2)

    if [ -z "$marker" ] || [ -z "$command" ] || [ -z "$status" ]; then
        echo "Error: invalid or incomplete firmware response." >&2
        return 1
    fi

    if [ "$marker" -ne "$PIBRICK_CMD" ]; then
        echo "Error: invalid response marker: $marker" >&2
        return 1
    fi

    if [ "$command" -ne "$expected_cmd" ]; then
        echo "Error: invalid response command: $command" >&2
        return 1
    fi

    if [ "$status" -ne "$STATUS_OK" ]; then
        echo "Error: firmware returned status $status." >&2
        return 1
    fi

    return 0
}


# ------------------------------------------------------------
# Timeout
# ------------------------------------------------------------

timeout_get() {
    local hidraw="$1"

    send_and_read \
        "$hidraw" \
        "$PIBRICK_CMD" \
        "$CMD_TIMEOUT" \
        "$GET" || return 1

    if ! check_response "$CMD_TIMEOUT"; then
        rm -f "$RESPONSE_FILE"
        return 1
    fi

    local value
    value=$(response_byte 3)

    rm -f "$RESPONSE_FILE"
    RESPONSE_FILE=""

    if [ "$QUIET" = true ]; then
        echo "$value"
    else
        echo "Backlight timeout: ${value} seconds"
    fi
}


timeout_set() {
    local hidraw="$1"
    local seconds="$2"

    if ! [[ "$seconds" =~ ^[0-9]+$ ]] || [ "$seconds" -gt 255 ]; then
        echo "Error: timeout must be 0-255 seconds." >&2
        return 1
    fi

    send_and_read \
        "$hidraw" \
        "$PIBRICK_CMD" \
        "$CMD_TIMEOUT" \
        "$SET" \
        "$seconds" || return 1

    if ! check_response "$CMD_TIMEOUT"; then
        rm -f "$RESPONSE_FILE"
        return 1
    fi

    local value
    value=$(response_byte 3)

    rm -f "$RESPONSE_FILE"
    RESPONSE_FILE=""

    if [ "$QUIET" = true ]; then
        echo "$value"
    else
        echo "Timeout set to ${value} seconds."
    fi
}


# ------------------------------------------------------------
# Backlight
# ------------------------------------------------------------

backlight_get() {
    local hidraw="$1"

    send_and_read \
        "$hidraw" \
        "$PIBRICK_CMD" \
        "$CMD_BACKLIGHT" \
        "$GET" || return 1

    if ! check_response "$CMD_BACKLIGHT"; then
        rm -f "$RESPONSE_FILE"
        return 1
    fi

    local level
    level=$(response_byte 3)

    rm -f "$RESPONSE_FILE"
    RESPONSE_FILE=""

    if [ "$QUIET" = true ]; then
        echo "$level"
    else
        echo "Backlight level: $level"
    fi
}


backlight_set() {
    local hidraw="$1"
    local level="$2"

    if ! [[ "$level" =~ ^[0-8]+$ ]] || [ "$level" -gt 8 ]; then
        echo "Error: backlight level must be 0-8." >&2
        return 1
    fi

    send_and_read \
        "$hidraw" \
        "$PIBRICK_CMD" \
        "$CMD_BACKLIGHT" \
        "$SET" \
        "$level" || return 1

    if ! check_response "$CMD_BACKLIGHT"; then
        rm -f "$RESPONSE_FILE"
        return 1
    fi

    local value
    value=$(response_byte 3)

    rm -f "$RESPONSE_FILE"
    RESPONSE_FILE=""

    if [ "$QUIET" = true ]; then
        echo "$value"
    else
        echo "Backlight level set to $value."
    fi
}


# ------------------------------------------------------------
# RGB
#
# rgb <RRGGBB>
# rgb <RRGGBB> <milliseconds>
# rgb 0
# ------------------------------------------------------------

rgb_set() {
    local hidraw="$1"
    local color="$2"
    local duration="${3:-0}"

    if [ "$color" = "0" ]; then
        color="000000"
    fi

    if ! [[ "$color" =~ ^[0-9a-fA-F]{6}$ ]]; then
        echo "Error: RGB color must be RRGGBB." >&2
        return 1
    fi

    if ! [[ "$duration" =~ ^[0-9]+$ ]] || [ "$duration" -gt 65535 ]; then
        echo "Error: RGB duration must be 0-65535 ms." >&2
        return 1
    fi

    local r=$((16#${color:0:2}))
    local g=$((16#${color:2:2}))
    local b=$((16#${color:4:2}))

    local ms_low=$((duration & 0xFF))
    local ms_high=$(((duration >> 8) & 0xFF))

    send_and_read \
        "$hidraw" \
        "$PIBRICK_CMD" \
        "$CMD_RGB" \
        "$r" \
        "$g" \
        "$b" \
        "$ms_low" \
        "$ms_high" || return 1

    if ! check_response "$CMD_RGB"; then
        rm -f "$RESPONSE_FILE"
        return 1
    fi

    rm -f "$RESPONSE_FILE"
    RESPONSE_FILE=""

    if [ "$QUIET" = true ]; then
        return 0
    fi

    if [ "$duration" -gt 0 ]; then
        echo "RGB set to #$color for ${duration}ms."
    elif [ "$color" = "000000" ]; then
        echo "RGB turned off."
    else
        echo "RGB set to #$color."
    fi
}


# ------------------------------------------------------------
# Main
# ------------------------------------------------------------

main() {

    # Parse options
    while [[ "$1" == -* ]]; do
        case "$1" in
            -q|--quiet)
                QUIET=true
                shift
                ;;

            -h|--help)
                usage
                exit 0
                ;;

            *)
                echo "Error: unknown option: $1" >&2
                usage >&2
                exit 1
                ;;
        esac
    done

    local command="$1"
    local arg1="$2"
    local arg2="$3"

    local hidraw

    hidraw=$(find_hidraw)

    if [ -z "$hidraw" ]; then
        echo "Error: piBrick Vial Raw HID device not found." >&2
        exit 1
    fi

    if [ "$QUIET" != true ]; then
        echo "Using $hidraw"
    fi

    case "$command" in

        timeout)
            if [ -z "$arg1" ]; then
                timeout_get "$hidraw"
            else
                timeout_set "$hidraw" "$arg1"
            fi
            ;;

        backlight)
            if [ -z "$arg1" ]; then
                backlight_get "$hidraw"
            else
                backlight_set "$hidraw" "$arg1"
            fi
            ;;

        rgb)
            if [ -z "$arg1" ]; then
                usage >&2
                exit 1
            fi

            if [ -n "$arg2" ] && [ -n "$4" ]; then
                echo "Error: too many arguments." >&2
                exit 1
            fi

            rgb_set "$hidraw" "$arg1" "$arg2"
            ;;

        *)
            usage >&2
            exit 1
            ;;

    esac
}


usage() {
    echo "Usage:"
    echo
    echo "  $0 [-q] timeout"
    echo "  $0 [-q] timeout <seconds>"
    echo
    echo "  $0 [-q] backlight"
    echo "  $0 [-q] backlight <0-8>"
    echo
    echo "  $0 [-q] rgb <RRGGBB>"
    echo "  $0 [-q] rgb <RRGGBB> <milliseconds>"
    echo "  $0 [-q] rgb 0"
    echo
    echo "Options:"
    echo "  -q, --quiet    Output only the requested value"
}


main "$@"
