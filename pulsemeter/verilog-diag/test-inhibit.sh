#!/bin/sh

INH_STS=79
INH_SET=83
INH_CLR=82

CHIP=gpiochip1

pulse_negedge() {
    gpioset $CHIP $1=0
    gpioget $CHIP $1 > /dev/null
}

initial_setup() {
    gpioget $CHIP $INH_STS > /dev/null
    gpioget $CHIP $INH_CLR > /dev/null
    gpioget $CHIP $INH_SET > /dev/null
    pulse_negedge $INH_CLR
}

inhibit_sts() {
    gpioget $CHIP $INH_STS
}

inhibit_set() {
    pulse_negedge $INH_SET
}

inhibit_clr() {
    pulse_negedge $INH_CLR
}


# called as
#    inhibit_test "[0|1]" "message"
inhibit_test() {
    local sts
    sts=$(inhibit_sts)


    if [ "${sts}" != "$1" ] ; then
        echo "${2} fail! sts=${sts}"
        return 1
    fi
    echo "${2} OK! sts=${sts}"
}

initial_setup
inhibit_test 0 "Initial setup"

for x in `seq 0 9` ; do
echo "Test iteration: $x"
    inhibit_set
    inhibit_test 1 "Inhibit set"

    inhibit_clr
    inhibit_test 0 "Inhibit clr"
done


