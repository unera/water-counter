#include "fiber.h"


static void
led_on(void)
{
	while (1){
		delay(300);
		digitalWrite(LED_BUILTIN, HIGH);
		fiber_cede();
	}
}

static void
led_off()
{
	while(1) {
		delay(50);
		digitalWrite(LED_BUILTIN, LOW);
		fiber_cede();
	}
}

void
setup() {
	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, LOW);
	fibers_init();

	static uint8_t stack1[64];
	static uint8_t stack2[64];
	fiber_create(led_on, stack1, sizeof(stack1));
	fiber_create(led_off, stack2, sizeof(stack2));
}

void
loop() {
	fiber_cancel(fiber_current());
}
