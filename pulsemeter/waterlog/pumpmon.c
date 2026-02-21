#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

struct pumpmon {
	FILE *pumplf; // pump event log
	FILE *flowlf; // water flow (meter) log
	struct gpiod_chip *chip;
	struct gpiod_line *iosel;
	struct gpiod_line *iosts;

	struct gpiod_line *inhsts; // inhibit status
	struct gpiod_line *inhset; // inhibit assert
	struct gpiod_line *inhclr; // inhibit clear
};

int
pulse_line(struct gpiod_line *line)
{
	int rc = 0;
	gpiod_line_release(line);
	rc += gpiod_line_request_output(line, "pumpmon", 0);
	gpiod_line_release(line);
	rc += gpiod_line_request_input(line, "pumpmon");
	return rc;
}

int
main(int argc, char **argv)
{
	const char *chipname = "gpiochip1";
	const char *pumplog = "/data/pumpmon.csv";

	struct pumpmon pmon;
	memset(&pmon, 0, sizeof(pmon));
	pmon.chip = gpiod_chip_open_by_name(chipname);
	pmon.iosel = gpiod_chip_get_line(pmon.chip, 95);
	pmon.iosts = gpiod_chip_get_line(pmon.chip, 96);
	
	pmon.inhsts = gpiod_chip_get_line(pmon.chip, 79);
	pmon.inhclr = gpiod_chip_get_line(pmon.chip, 82);
	pmon.inhset = gpiod_chip_get_line(pmon.chip, 83);
	

	if (!pmon.chip) {
		fprintf(stderr, "Unable to open gpiochip\n");
		return 1;
	}

	if (!pmon.iosel) {
		fprintf(stderr, "Unable to open iosel\n");
		return 1;
	}

	if (!pmon.iosts) {
		fprintf(stderr, "Unable to open iosts\n");
		return 1;
	}
	
	if (gpiod_line_request_input(pmon.iosel, "pumpmon")) {
		fprintf(stderr, "Unable to request iosel input\n");
		return 1;
	}

	if (gpiod_line_request_input(pmon.iosts, "pumpmon")) {
		fprintf(stderr, "Unable to request iosts input\n");
		return 1;
	}

	if (gpiod_line_request_input(pmon.inhsts, "pumpmon")) {
		fprintf(stderr, "Unable to open inhibit status input\n");
		return 1;
	}

	if (gpiod_line_request_input(pmon.inhclr, "pumpmon")) {
		fprintf(stderr, "Unable to open inhibit clear input\n");
		return 1;
	}

	if (gpiod_line_request_input(pmon.inhset, "pumpmon")) {
		fprintf(stderr, "Unable to open inhibit set input\n");
		return 1;
	}

	pmon.pumplf = fopen(pumplog, "a");
	if (!pmon.pumplf) {
		fprintf(stderr, "Unable to open log file: %s\n", pumplog);
		return 1;
	}

	int pump_sts = gpiod_line_get_value(pmon.iosts);
	int pump_sts_last = pump_sts;

	int inhibit_sts = gpiod_line_get_value(pmon.inhsts);
	int inhibit_sts_last = inhibit_sts;
	
	while (true) {
		time_t now = time(NULL);

		pump_sts = gpiod_line_get_value(pmon.iosts);
		if (pump_sts != pump_sts_last) {
			fprintf(pmon.pumplf,
				"%s, %s\n", strtok(ctime(&now), "\n"), pump_sts ? "pump_on" : "pump_off");
			fflush(pmon.pumplf);
		}

		inhibit_sts = gpiod_line_get_value(pmon.inhsts);
		if (inhibit_sts != inhibit_sts_last) {
			fprintf(pmon.pumplf,
				"%s, %s\n", strtok(ctime(&now), "\n"), inhibit_sts ? "inhibit_on" : "inhibit_off");
			fflush(pmon.pumplf);
		}

		/* If the trigger file exists then pulse the inhibit enable signal, else pulse the clear */ 
		struct stat sb;
		memset(&sb, 0, sizeof(sb));
		if (stat("/data/.inhibit", &sb) == 0) {
			pulse_line(pmon.inhset);
		} else {
			pulse_line(pmon.inhclr);
		}

		pump_sts_last = pump_sts;
		inhibit_sts_last = inhibit_sts;
		sleep(1);
	}
	
	return 0;
}

