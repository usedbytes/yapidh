#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "platform.h"
#include "types.h"
#include "wave_gen.h"

struct square_wave_source {
	struct source base;

	int pin;
	int period;
	bool rising;
};

static int square_wave_source_delay(struct source *s)
{
	struct square_wave_source *ss = (struct square_wave_source *)s;

	/* Next event is always period / 2 ticks away */
	return ss->period / 2;
}

static void square_wave_source_event(struct source *s, struct event *ev)
{
	struct square_wave_source *ss = (struct square_wave_source *)s;

	ev->channel = ss->pin;
	if (ss->rising) {
		ev->type = EVENT_RISING_EDGE;
	} else {
		ev->type = EVENT_FALLING_EDGE;
	}
	ss->rising = !ss->rising;
}

int main(int argc, char *argv[])
{
	int ret = 0;

	struct square_wave_source sq_1kHz = {
		.base = {
			.get_delay = square_wave_source_delay,
			.gen_event = square_wave_source_event,
		},
		/* 10us tick by default - 100 * 10us = 1ms, 1 kHz */
		.period = 100,
		.pin = 16,
	};

	struct square_wave_source sq_3_33kHz = {
		.base = {
			.get_delay = square_wave_source_delay,
			.gen_event = square_wave_source_event,
		},
		/* 10us tick by default - 30 * 10us = 300 us, 3.333 kHz */
		.period = 30,
		.pin = 19,

		/* Start this wave out-of-phase */
		.rising = true,
	};

	struct wave_ctx ctx = {
		.n_sources = 2,
		.sources = {
			&sq_1kHz.base,
			&sq_3_33kHz.base,
		},
	};
	uint32_t pins = (1 << 16) | (1 << 19);

	struct platform *p = platform_init(pins);
	if (!p) {
		fprintf(stderr, "Platform creation failed\n");
		return 1;
	}

	ctx.be = platform_get_backend(p);

	while (1) {
		ret = platform_sync(p, 1000);
		if (ret) {
			fprintf(stderr, "Timeout waiting for fence\n");
			goto fail;
		}

		wave_gen(&ctx, 1600);
	}

fail:
	platform_fini(p);
	return ret;
}
