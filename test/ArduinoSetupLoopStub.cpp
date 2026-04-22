#if defined(ARDUINO)

#include <Arduino.h>

// Weak fallback for Arduino unit-test suites that only provide native-style main().
// Real setup()/loop() implementations (for example in test_ESP32) override these.
extern int main(int argc, char **argv) __attribute__((weak));
void setup() __attribute__((weak));
void loop() __attribute__((weak));

void setup() {
	if (main) {
		static char kArg0[] = "pio-test";
		static char *kArgv[] = {kArg0, nullptr};
		main(1, kArgv);
	}
}

void loop() {
	delay(10);
}

#endif
