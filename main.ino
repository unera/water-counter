#include <avr/interrupt.h>
#include <util/atomic.h>

#define FIBER_STACK_SIZE	64
#include "fiber.h"


#define F_CLOCK		128

static struct fiber *_delay[5] = {NULL, NULL, NULL, NULL, NULL};
#define SLOTS 5


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
led_X(void *data)
{
	uint8_t led = ((uint16_t)data) >> 8;
	uint8_t period = ((uint16_t)data) & 0xFF;
	pinMode(led, OUTPUT);
	while (1){
		fiber_delay(period);
		led_switch(led);
	}
}


void
setup() {
	pinMode(LED_BUILTIN, OUTPUT);

	pinMode(7, OUTPUT);
	pinMode(8, OUTPUT);
	pinMode(9, OUTPUT);


	OCR2A = F_CPU / 1024 /  F_CLOCK;
	TCCR2A = (1 << WGM21) | (0 << WGM20);			// CTC to OCR2A
	TCCR2B = (1 << CS22) | (1 << CS21) | (1 << CS20);	// 1024
	TIMSK2 = (1 << OCIE2A);
	sei();

	fibers_init();
	// FIBER_CREATE(fiber_cede, 64);
	FIBER(led_X, (void *)((7 << 8) | 21));
	FIBER(led_X, (void *)((8 << 8) | 22));
	FIBER(led_X, (void *)((9 << 8) | 23));
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
	fiber_schedule();
	//fiber_cancel(fiber_current());
	// fiber_cede();
}
