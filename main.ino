#include "fiber.h"
#include <avr/interrupt.h>
#include <util/atomic.h>

#define F_CLOCK		128

static struct fiber *_delay[5] = {NULL, NULL, NULL, NULL, NULL};
#define SLOTS 5

static struct fiber *off, *on;

inline void led_switch(uint8_t no) {
	if (digitalRead(no))
		digitalWrite(no, LOW);
	else
		digitalWrite(no, HIGH);
}

void
fiber_delay(uint8_t clocks)
{
	while(clocks--) {
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
			for (uint8_t i = 0; i < SLOTS; i++) {
				if (!_delay[i]) {
					_delay[i] = fiber_current();
					break;
				}
			}
		}
		fiber_schedule();
	}
}


static void
led_on(void)
{
	while (1){
		fiber_delay(23);
		led_switch(8);
		digitalWrite(LED_BUILTIN, HIGH);
	}
}

static void
led_off()
{
	while(1) {
		fiber_delay(21);
		led_switch(7);
		digitalWrite(LED_BUILTIN, LOW);
	}
}

void
setup() {
	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, LOW);

	pinMode(7, OUTPUT);
	pinMode(8, OUTPUT);
	pinMode(9, OUTPUT);

	fibers_init();

	static uint8_t stack1[128];
	static uint8_t stack2[128];

	OCR2A = F_CPU / 1024 /  F_CLOCK;
	TCCR2A = (1 << WGM21) | (0 << WGM20);			// CTC to OCR2A
	TCCR2B = (1 << CS22) | (1 << CS21) | (1 << CS20);	// 1024
	TIMSK2 = (1 << OCIE2A);
	sei();

	off = fiber_create(led_off, stack2, sizeof(stack2));
	on = fiber_create(led_on, stack1, sizeof(stack1));
}

SIGNAL(TIMER2_COMPA_vect) {
	TCNT2 = 0;
	for (uint8_t i = 0; i < SLOTS; i++) {
		if (!_delay[i])
			continue;
		if (fiber_status(_delay[i]) != 's')
			continue;
		fiber_wakeup(_delay[i]);
		_delay[i] = NULL;
	}
}

void
loop() {
	fiber_cede();
}
