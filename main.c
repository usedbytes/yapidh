#include <stdio.h>
#include <stdint.h>
#include <math.h>

struct ctx {
	double alpha;
	double accel;
	double f;

	double n;
	double target_n;

	double c;
};

double same_sign(double a, double b) {
	if (b < 0) {
		return a < 0 ? a : -a;
	}
	return a >= 0 ? a : -a;
}

void set_speed(struct ctx *c, double speed)
{
	c->target_n = (speed * speed) / (2 * c->alpha * c->accel);
	if (c->target_n < fabs(c->n)) {
		c->target_n = -c->target_n;
	}

	c->n = same_sign(c->n, c->target_n);

	if (c->n == 0.0f) {
		c->c = 0.676 * c->f * sqrt((2 * c->alpha) / c->accel);
		c->n = 1;
	}
}

void tick(struct ctx *c)
{
	if (c->n >= c->target_n) {
		if (c->target_n == 0.0f) {
			// Disable motor.
		}
		return;
	}

	c->c = c->c - ((2 * c->c) / ((4 * c->n) + 1));
	c->n++;
}

void dump_ctx(struct ctx *c)
{
	fprintf(stderr, "------\n");
	fprintf(stderr, "Alpha   : %4.3f\n", c->alpha);
	fprintf(stderr, "Accel   : %4.3f\n", c->accel);
	fprintf(stderr, "Freq    : %4.3f\n", c->f);
	fprintf(stderr, "n       : %4.3f\n", c->n);
	fprintf(stderr, "target_n: %4.3f\n", c->target_n);
	fprintf(stderr, "c       : %4.3f\n", c->c);
	fprintf(stderr, "speed   : %4.3f\n", (c->f * c->alpha) / round(c->c));
	fprintf(stderr, "------\n");
}

double calc_speed(struct ctx *c)
{
	return (c->f * c->alpha) / round(c->c);
}

int main(int argc, char *argv[])
{
	int i;
	struct ctx ctx = {
		.alpha = 2 * M_PI / 600.0f,
		.accel = 100,
		.f = 100000,
	};

	float t = 0.0f;

	set_speed(&ctx, 25);
	dump_ctx(&ctx);
	while(ctx.n < ctx.target_n) {
		t += ctx.c / ctx.f;
		tick(&ctx);
		printf("%f, %f\n", t, calc_speed(&ctx));
	}

	set_speed(&ctx, 20);
	dump_ctx(&ctx);
	while(ctx.n < ctx.target_n) {
		t += ctx.c / ctx.f;
		tick(&ctx);
		printf("%f, %f\n", t, calc_speed(&ctx));
	}

	set_speed(&ctx, 25);
	dump_ctx(&ctx);
	while(ctx.n < ctx.target_n) {
		t += ctx.c / ctx.f;
		tick(&ctx);
		printf("%f, %f\n", t, calc_speed(&ctx));
	}

	set_speed(&ctx, 0);
	dump_ctx(&ctx);
	while(ctx.n < ctx.target_n) {
		t += ctx.c / ctx.f;
		tick(&ctx);
		printf("%f, %f\n", t, calc_speed(&ctx));
	}

	return 0;
}
