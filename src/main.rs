use esp_idf_hal::{
    gpio::*,
    i2c::*,
    prelude::*,
    adc::*,
};
use esp_idf_sys as _;
use embedded_graphics::{
    pixelcolor::BinaryColor,
    prelude::*,
    primitives::{Circle, Line, PrimitiveStyle},
    text::{Text, TextStyle},
};
use sh1106::{prelude::*, Builder};
use std::time::Duration;
use std::thread;

// Watch parameters
const CENTER_X: i32 = 64;
const CENTER_Y: i32 = 32;
const WATCH_RADIUS: i32 = 25;
const HOUR_HAND_LENGTH: i32 = 15;
const MINUTE_HAND_LENGTH: i32 = 20;
const SECOND_HAND_LENGTH: i32 = 22;

// Joystick thresholds
const JOYSTICK_THRESHOLD: u32 = 2000;
const JOYSTICK_DEADZONE: u32 = 1000;

#[derive(Clone, Copy)]
enum SetState {
    DisplayTime,
    SetHour,
    SetMinute,
}

struct Watch {
    hours: u8,
    minutes: u8,
    seconds: u8,
    state: SetState,
    last_second: Duration,
    last_adjust: Duration,
    adjust_delay: Duration,
}

impl Watch {
    fn new() -> Self {
        Self {
            hours: 12,
            minutes: 0,
            seconds: 0,
            state: SetState::DisplayTime,
            last_second: Duration::from_secs(0),
            last_adjust: Duration::from_secs(0),
            adjust_delay: Duration::from_millis(200),
        }
    }

    fn update_time(&mut self, now: Duration) {
        if let SetState::DisplayTime = self.state {
            if now - self.last_second >= Duration::from_secs(1) {
                self.last_second = now;
                self.seconds += 1;
                if self.seconds >= 60 {
                    self.seconds = 0;
                    self.minutes += 1;
                    if self.minutes >= 60 {
                        self.minutes = 0;
                        self.hours += 1;
                        if self.hours >= 24 {
                            self.hours = 0;
                        }
                    }
                }
            }
        }
    }

    fn draw_watch<D: DrawTarget<Color = BinaryColor>>(&self, display: &mut D) -> Result<(), D::Error> {
        // Draw watch circle
        Circle::new(Point::new(CENTER_X, CENTER_Y), WATCH_RADIUS as u32)
            .into_styled(PrimitiveStyle::with_stroke(BinaryColor::On, 1))
            .draw(display)?;

        // Draw hour markers
        for i in 0..12 {
            let angle = (i as f32 * std::f32::consts::PI / 6.0) - std::f32::consts::PI / 2.0;
            let x1 = CENTER_X + ((WATCH_RADIUS - 3) as f32 * angle.cos()) as i32;
            let y1 = CENTER_Y + ((WATCH_RADIUS - 3) as f32 * angle.sin()) as i32;
            let x2 = CENTER_X + (WATCH_RADIUS as f32 * angle.cos()) as i32;
            let y2 = CENTER_Y + (WATCH_RADIUS as f32 * angle.sin()) as i32;
            
            Line::new(Point::new(x1, y1), Point::new(x2, y2))
                .into_styled(PrimitiveStyle::with_stroke(BinaryColor::On, 1))
                .draw(display)?;
        }

        // Calculate hand angles
        let hour_angle = ((self.hours % 12) as f32 + self.minutes as f32 / 60.0) * std::f32::consts::PI / 6.0 - std::f32::consts::PI / 2.0;
        let minute_angle = self.minutes as f32 * std::f32::consts::PI / 30.0 - std::f32::consts::PI / 2.0;
        let second_angle = self.seconds as f32 * std::f32::consts::PI / 30.0 - std::f32::consts::PI / 2.0;

        // Draw hands
        self.draw_hand(display, hour_angle, HOUR_HAND_LENGTH)?;
        self.draw_hand(display, minute_angle, MINUTE_HAND_LENGTH)?;
        self.draw_hand(display, second_angle, SECOND_HAND_LENGTH)?;

        // Draw center dot
        Circle::new(Point::new(CENTER_X, CENTER_Y), 2)
            .into_styled(PrimitiveStyle::with_fill(BinaryColor::On))
            .draw(display)?;

        // Draw mode indicator
        let mode_text = match self.state {
            SetState::DisplayTime => "Mode: Time",
            SetState::SetHour => "Mode: Set Hour",
            SetState::SetMinute => "Mode: Set Min",
        };

        Text::new(mode_text, Point::new(0, 0), TextStyle::default())
            .draw(display)?;

        Ok(())
    }

    fn draw_hand<D: DrawTarget<Color = BinaryColor>>(
        &self,
        display: &mut D,
        angle: f32,
        length: i32,
    ) -> Result<(), D::Error> {
        let end_x = CENTER_X + (length as f32 * angle.cos()) as i32;
        let end_y = CENTER_Y + (length as f32 * angle.sin()) as i32;

        Line::new(Point::new(CENTER_X, CENTER_Y), Point::new(end_x, end_y))
            .into_styled(PrimitiveStyle::with_stroke(BinaryColor::On, 1))
            .draw(display)
    }

    fn handle_joystick(&mut self, y_value: u32, button_pressed: bool, now: Duration) {
        if button_pressed {
            self.state = match self.state {
                SetState::DisplayTime => SetState::SetHour,
                SetState::SetHour => SetState::SetMinute,
                SetState::SetMinute => SetState::DisplayTime,
            };
        }

        if self.state != SetState::DisplayTime && now - self.last_adjust >= self.adjust_delay {
            if y_value < JOYSTICK_THRESHOLD - JOYSTICK_DEADZONE {
                match self.state {
                    SetState::SetHour => self.hours = (self.hours + 1) % 24,
                    SetState::SetMinute => self.minutes = (self.minutes + 1) % 60,
                    _ => {}
                }
                self.last_adjust = now;
            } else if y_value > JOYSTICK_THRESHOLD + JOYSTICK_DEADZONE {
                match self.state {
                    SetState::SetHour => self.hours = (self.hours + 23) % 24,
                    SetState::SetMinute => self.minutes = (self.minutes + 59) % 60,
                    _ => {}
                }
                self.last_adjust = now;
            }
        }
    }
}

fn main() -> anyhow::Result<()> {
    esp_idf_sys::link_patches();
    
    let peripherals = Peripherals::take().unwrap();
    let pins = peripherals.pins;

    // Initialize I2C
    let i2c = I2cDriver::new(
        peripherals.i2c0,
        pins.gpio21, // SDA
        pins.gpio22, // SCL
        &I2cConfig::new().baudrate(Hertz(400_000)),
    )?;

    // Initialize OLED display
    let mut display: GraphicsMode<_> = Builder::new()
        .with_i2c_addr(0x3C)
        .with_size(DisplaySize::Display128x64)
        .connect_i2c(i2c)
        .into();

    display.init()?;
    display.flush()?;

    // Initialize ADC for joystick
    let adc = AdcDriver::new(peripherals.adc1)?;
    let y_pin = pins.gpio26.into_analog()?;
    let button_pin = pins.gpio25.into_input()?;

    let mut watch = Watch::new();
    let mut start_time = std::time::Instant::now();

    loop {
        let now = start_time.elapsed();
        
        // Read joystick
        let y_value = adc.read(&y_pin, None)?;
        let button_pressed = button_pin.is_low()?;

        // Update watch state
        watch.update_time(now);
        watch.handle_joystick(y_value, button_pressed, now);

        // Draw watch
        display.clear();
        watch.draw_watch(&mut display)?;
        display.flush()?;

        thread::sleep(Duration::from_millis(50));
    }
} 