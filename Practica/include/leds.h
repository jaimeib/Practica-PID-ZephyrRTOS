/* LEDS CONFIGURATION*/

typedef enum { RED1, GREEN1, GREEN2 } led_number_t;

#define BLINK_LED_PERIOD_MS 1000

#define LED0 DT_ALIAS(led0)
#define LED1 DT_ALIAS(led1)
#define LED2 DT_ALIAS(led2)

#if !DT_NODE_HAS_STATUS(LED0, okay)
#error "Unsupported board: led0 devicetree alias is not defined"
#endif

#if !DT_NODE_HAS_STATUS(LED1, okay)
#error "Unsupported board: led1 devicetree alias is not defined"
#endif

#if !DT_NODE_HAS_STATUS(LED2, okay)
#error "Unsupported board: led2 devicetree alias is not defined"
#endif

struct led {
	struct gpio_dt_spec spec;
	const char *gpio_pin_name;
};

static const struct led led0 = {
	.spec = GPIO_DT_SPEC_GET_OR(LED0, gpios, { 0 }),
	.gpio_pin_name = DT_PROP_OR(LED0, label, "RED1"),
};

static const struct led led1 = {
	.spec = GPIO_DT_SPEC_GET_OR(LED1, gpios, { 0 }),
	.gpio_pin_name = DT_PROP_OR(LED1, label, "GREEN1"),
};

static const struct led led2 = {
	.spec = GPIO_DT_SPEC_GET_OR(LED2, gpios, { 0 }),
	.gpio_pin_name = DT_PROP_OR(LED2, label, "GREEN2"),
};

//LED FUNCTIONS:
void blink_function(const struct led *led, uint32_t sleep_ms);
void turn_function(const struct led *led, int status);

void blink_led(led_number_t led_to_blink);
void turn_led(led_number_t led_to_turn, int status);