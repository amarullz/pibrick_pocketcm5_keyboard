#!/bin/bash

VID="F10C"
PID="0001"
INTERFACE="01"

REPORT_SIZE=32

PIBRICK_CMD=0xFF

CMD_TIMEOUT=0x01
CMD_BACKLIGHT=0x02
CMD_RGB=0x03

GET=0x00
SET=0x01

STATUS_OK=0
STATUS_ERROR=1


find_hidraw() {
    for hid in /sys/bus/hid/devices/0003:${VID}:${PID}.*; do
        [ -d "$hid" ] || continue

        local interface

        interface=$(udevadm info -a -p "$hid" 2>/dev/null |
            grep -m1 'ATTRS{bInterfaceNumber}' |
            sed -E 's/.*=="([^"]+)".*/\1/')

        [ "$interface" = "$INTERFACE" ] || continue

        for raw in "$hid"/hidraw/hidraw*; do
            [ -e "$raw" ] || continue

            echo "/dev/$(basename "$raw")"
            return 0
        done
    done

    return 1
}


send_report() {
    local hidraw="$1"
    shift

    local report=""

    for byte in "$@"; do
        printf -v hex '%02x' "$byte"
        report+="\\x$hex"
    done

    local count=$#

    for ((i=count; i<REPORT_SIZE; i++)); do
        report+="\\x00"
    done

    printf '%b' "$report" > "$hidraw"
}


send_and_read() {
    local hidraw="$1"
    shift

    local response_file
    response_file=$(mktemp)

    (
        timeout 2 dd \
            if="$hidraw" \
            of="$response_file" \
            bs="$REPORT_SIZE" \
            count=1 \
            2>/dev/null
    ) &

    local reader_pid=$!

    sleep 0.05

    send_report "$hidraw" "$@"

    wait "$reader_pid"

    if [ ! -s "$response_file" ]; then
        rm -f "$response_file"
        echo "Error: no response received."
        return 1
    fi

    RESPONSE_FILE="$response_file"
}


timeout_get() {
    local hidraw="$1"

    send_and_read \
        "$hidraw" \
        "$PIBRICK_CMD" \
        "$CMD_TIMEOUT" \
        "$GET" || return 1

    local status
    local value

    status=$(od -An -tu1 -j2 -N1 "$RESPONSE_FILE" | tr -d ' ')
    value=$(od -An -tu1 -j3 -N1 "$RESPONSE_FILE" | tr -d ' ')

    rm -f "$RESPONSE_FILE"

    if [ "$status" != "$STATUS_OK" ]; then
        echo "Error: firmware returned status $status."
        return 1
    fi

    echo "Backlight timeout: ${value} seconds"
}


timeout_set() {
    local hidraw="$1"
    local seconds="$2"

    if ! [[ "$seconds" =~ ^[0-9]+$ ]] || [ "$seconds" -gt 255 ]; then
        echo "Error: timeout must be 0-255 seconds."
        return 1
    fi

    send_report \
        "$hidraw" \
        "$PIBRICK_CMD" \
        "$CMD_TIMEOUT" \
        "$SET" \
        "$seconds"

    echo "Timeout set to ${seconds} seconds."
}


backlight_get() {
    local hidraw="$1"

    send_and_read \
        "$hidraw" \
        "$PIBRICK_CMD" \
        "$CMD_BACKLIGHT" \
        "$GET" || return 1

    local status
    local level

    status=$(od -An -tu1 -j2 -N1 "$RESPONSE_FILE" | tr -d ' ')
    level=$(od -An -tu1 -j3 -N1 "$RESPONSE_FILE" | tr -d ' ')

    rm -f "$RESPONSE_FILE"

    if [ "$status" != "$STATUS_OK" ]; then
        echo "Error: firmware returned status $status."
        return 1
    fi

    echo "Backlight level: $level"
}


backlight_set() {
    local hidraw="$1"
    local level="$2"

    if ! [[ "$level" =~ ^[0-8]+$ ]] || [ "$level" -gt 8 ]; then
        echo "Error: backlight level must be 0-8."
        return 1
    fi

    send_report \
        "$hidraw" \
        "$PIBRICK_CMD" \
        "$CMD_BACKLIGHT" \
        "$SET" \
        "$level"

    echo "Backlight level set to $level."
}


rgb_set() {
    local hidraw="$1"
    local color="$2"
    local duration="$3"

    if [ "$color" = "0" ]; then
        color="000000"
    fi

    if ! [[ "$color" =~ ^[0-9a-fA-F]{6}$ ]]; then
        echo "Error: RGB color must be RRGGBB."
        return 1
    fi

    local r=$((16#${color:0:2}))
    local g=$((16#${color:2:2}))
    local b=$((16#${color:4:2}))

    local ms=0

    if [ -n "$duration" ]; then
        if ! [[ "$duration" =~ ^[0-9]+$ ]] || [ "$duration" -gt 65535 ]; then
            echo "Error: RGB duration must be 0-65535 ms."
            return 1
        fi

        ms="$duration"
    fi

    local ms_low=$((ms & 0xFF))
    local ms_high=$(((ms >> 8) & 0xFF))

    send_report \
        "$hidraw" \
        "$PIBRICK_CMD" \
        "$CMD_RGB" \
        "$r" \
        "$g" \
        "$b" \
        "$ms_low" \
        "$ms_high"

    if [ "$ms" -gt 0 ]; then
        echo "RGB set to #$color for ${ms}ms."
    elif [ "$color" = "000000" ]; then
        echo "RGB turned off."
    else
        echo "RGB set to #$color."
    fi
}


main() {
    local hidraw

    hidraw=$(find_hidraw)

    if [ -z "$hidraw" ]; then
        echo "Error: piBrick Vial Raw HID device not found."
        exit 1
    fi

    echo "Using $hidraw"

    case "$1" in

        timeout)
            if [ -z "$2" ]; then
                timeout_get "$hidraw"
            else
                timeout_set "$hidraw" "$2"
            fi
            ;;

        backlight)
            if [ -z "$2" ]; then
                backlight_get "$hidraw"
            else
                backlight_set "$hidraw" "$2"
            fi
            ;;

        rgb)
            if [ -z "$2" ]; then
                echo "Usage:"
                echo "  $0 rgb <RRGGBB>"
                echo "  $0 rgb <RRGGBB> <milliseconds>"
                echo "  $0 rgb 0"
                exit 1
            fi

            rgb_set "$hidraw" "$2" "$3"
            ;;

        *)
            echo "Usage:"
            echo "  $0 timeout"
            echo "  $0 timeout <seconds>"
            echo
            echo "  $0 backlight"
            echo "  $0 backlight <0-8>"
            echo
            echo "  $0 rgb <RRGGBB>"
            echo "  $0 rgb <RRGGBB> <milliseconds>"
            echo "  $0 rgb 0"
            exit 1
            ;;
    esac
}

main "$@"
