# LCD Wiki ES3C28P / ES3N28P

2.8-inch ESP32-S3 display module.

Documentation: [https://www.lcdwiki.com/2.8inch_ESP32-S3_Display](https://www.lcdwiki.com/2.8inch_ESP32-S3_Display)

One firmware covers both SKUs: ES3C28P has a capacitive touch panel, ES3N28P
does not. Touch is detected at startup and skipped when absent.

## Specifications

| Item | Value |
|------|-------|
| SoC | ESP32-S3, 16 MB flash, 8 MB OPI PSRAM |
| Display | 2.8" IPS, 240x320, ILI9341V over SPI |
| Touch | FT6336G capacitive, I2C 0x38 (ES3C28P only) |
| Audio | ES8311 codec, MEMS microphone, speaker amplifier |
| Other | RGB LED, MicroSD, battery charging with voltage ADC |

## Pins

| Function | GPIO |
|----------|------|
| LCD CS / DC / SCK / MOSI / MISO | 10 / 46 / 12 / 11 / 13 |
| LCD backlight | 45 (high = on) |
| LCD reset | shared with the ESP32-S3 EN reset |
| I2C SDA / SCL (codec, touch) | 16 / 15 |
| Touch reset / interrupt | 18 / 17 |
| I2S MCLK / BCLK / WS / DOUT / DIN | 4 / 5 / 7 / 8 / 6 |
| Amplifier enable | 1 (low = on) |
| RGB LED | 42 |
| Battery ADC | 9 |
| BOOT button | 0 |

## Home screen and apps

On boards with touch (ES3C28P) the device powers on to a welcome screen
(a robot face, "hello" and "please wait"). Once startup is done it opens a home
screen with five apps; when activation or WiFi setup needs the user, the
chatbot screen opens instead. The volume button at the top left of the home
screen shows the speaker volume; tap it for a panel with a slider and
-/+ buttons (steps of 10, a beep at the new level). The volume is saved, and is
the same one the chatbot changes by voice. The apps are (`main/apps/launcher.*`):

- **Chatbot**: the voice assistant with the robot eyes. A home button sits in
  the bottom-left corner. A conversation started from the home screen (wake
  word or BOOT) opens this app automatically.
- **Music**: plays MP3, M4A/AAC, FLAC and WAV files from the MicroSD card.
  It opens on a list of the folders that hold music (up to three levels deep;
  songs at the top of the card show as "SD card"). Tap a folder for its songs
  and a song to play it; previous and next stay in that folder. The back
  arrow returns to the folders without stopping the song. The bar below has
  previous, play/pause, next and volume. BOOT is play/pause
  while this app is open. The chatbot is paused here (no wake word) so it
  never talks over the music; leaving the app stops playback and resumes it.

- **Games**: fifteen one-finger games in a menu that scrolls
  (`main/apps/games_app.*`). The chatbot is paused here too, and leaving the app
  ends the game.
  - *Tic-Tac-Toe*: your star against the robot, taking turns to go first. The
    robot sometimes misses a block, so children can win.
  - *Memory*: find the six pairs among twelve cards; the top bar counts moves.
  - *Snake*: tap above, below or beside the snake to turn that way. It speeds
    up as it grows. The top bar shows score/best, and the best score is kept.
  - *Copy Me*: watch the pads light up, then tap them in the same order. The
    sequence grows by one each round; the best score is kept.
  - *Bop the Bot*: tap the robots before they hide, in a 30-second round that
    gets faster. The bar under the title shows the time left; the best score
    is kept.
  - *Slide Puzzle*: slide the tiles next to the gap to put 1 to 8 back in order.
    The top bar counts moves.
  - *Four in a Row*: tap a column to drop your yellow disc, against the robot's
    blue ones, taking turns to go first. The robot sometimes misses a block.
  - *Quick Sums*: ten sums with three answers each, up to 10 and then up to 20.
    Only a right first try scores.
  - *Catch Stars*: slide a finger to move the basket. Dropping a star or catching
    a red mark costs one of three lives; the best score is kept.
  - *Robot Hop*: tap to jump over the blocks; it speeds up as you go. The best
    score is kept.
  - *Odd One Out*: tap the tile with a different color. The difference shrinks
    each round, one wrong tap ends the game, and the best score is kept.
  - *Lights Out*: a tap flips a light and the ones next to it; turn them all
    off. Each level starts further from all-off, the reached level is kept,
    and the refresh button tries the same puzzle again.
  - *Number Merge*: swipe the board, or use the arrow buttons, to slide the
    tiles; equal numbers that meet add up. Over when nothing can move; the best
    score is kept.
  - *How Many?*: count the pictures and tap the number, ten questions (up to 5
    pictures, then up to 10). Only a right first try scores.
  - *Balloon Pop*: tap the balloons before they float away. Three may escape;
    they rise faster as the score grows, and the best score is kept.
- **English**: English words for children, with Vietnamese meanings
  (`main/apps/english_app.*`, words in `english_words.cc`). Thirty
  topics in a menu that scrolls: colors, numbers, things, animals, fruits, the
  body, food, vegetables, vehicles, nature, clothes, family, school, toys, jobs,
  feelings, the home, sports, sea animals, insects, weather, shapes, actions,
  musical instruments, birds, places, summer, winter, the zoo and space. A
  tile shows its topic's first picture. Each topic shows word
  cards (a color swatch, a digit or a picture, the English word and its
  meaning) and says the word; tap the picture to hear it again, and the arrows
  for the other cards. The mic button opens the chatbot, listening, to practise
  saying the word; the home button there ends the conversation, and the
  English tile brings the topic back. The quiz button starts ten questions:
  pick the English word for the picture and meaning. A wrong pick turns red
  and can be retried, but only a right first try scores; a right pick says the
  word. Words are not spoken during a conversation.
  The word clips and pictures live in `main/apps/english_assets/` and are
  packed into the assets partition: clips (about 2 KB each) from
  `scripts/gen_english_audio.py` (edge-tts, voice `en-US-AnaNeural`), pictures
  (88x88 RGB565A8, 23 KB each) from `scripts/gen_english_images.py` (Noto Color
  Emoji, Apache License 2.0). After adding a word, give it an emoji in that
  script, run both scripts and flash the assets too. Without the pictures a
  card falls back to an icon or the word's first letter.
  The chatbot stays on here, so a wake word opens it. English words use the
  built-in Montserrat 28 font (`CONFIG_LV_FONT_MONTSERRAT_28`).
- **Calculator**: a keypad with + - x : (x and : are worked out before + and
  -, as at school), decimals, C to clear and a delete key. The line above the
  sum shows the answer so far; dividing by 0 shows a message. The rules are in
  `main/apps/calculator.*` with a host test. A wake word opens the chatbot.

The SD card must be FAT32. File names may use Vietnamese and other non-ASCII
characters (the board enables UTF-8 file names).

## Display

The screen shows animated robot eyes (`main/display/eye_display.h`, shared
with the bread-compact boards) that react to the emotion of each reply, with
the usual status bar and chat text underneath.

## Controls

- BOOT button: start or stop a conversation. Press during startup to enter
  WiFi configuration.
- Talk button (ES3C28P): the round mic button in the bottom-right corner
  does the same as BOOT. Blue is ready, green is listening, orange is the
  bot speaking (a tap interrupts), grey is busy. Taps elsewhere are ignored.
- While a song or story plays the button turns red with a stop icon; tap it
  (or press BOOT) to stop playback.
- Hold anywhere on the screen for 3 seconds to enter WiFi configuration.

## Battery

The top-right corner shows the battery icon and percentage, refreshed every
10 seconds: red at 15% or below, a green bolt while charging. The TP4054
charge status is not wired to a GPIO, so charging is inferred from the
voltage trend and can take a minute to show. Without a battery fitted the
reading follows the charger voltage.

## Build

```bash
python3 scripts/build.py lcdwiki-es3c28p
```

The pinout matches `freenove-esp32s3-display-2.8-lcd`, and this board reuses
its driver code. It has its own board type so OTA updates for one product are
never offered to the other.
