#include "fiber.h"


static void
led_on(void)
{
	for(;;) {
		delay(300);
		digitalWrite(LED_BUILTIN, HIGH);
	}
}

static void
led_off()
{
	for(;;) {
		delay(500);
		digitalWrite(LED_BUILTIN, LOW);
	}
}

void
setup() {
	pinMode(LED_BUILTIN, OUTPUT);
	fibers_init();

	static uint8_t stack1[64];
	static uint8_t stack2[64];
	fiber_create(led_on, stack1, sizeof(stack1));
	fiber_create(led_off, stack2, sizeof(stack2));
}

void
loop() {
	fiber_cede();
}
