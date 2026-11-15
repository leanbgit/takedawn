Takedawn @VERSION@ for Windows (x64): four-player split screen

An unofficial build of Takedawn @UPVERSION@ with a four-player split screen
for the North America (NTSC-U) disc. It is not made or supported by the
Takedawn project; report problems on github.com/leanbgit/takedawn, not
upstream. No game data is included: you need your own disc.

Windows 10 or 11, 64-bit, and a graphics driver with Vulkan (any current
AMD, Intel or NVIDIA driver). Four views cost more than two: four players
at 60 fps want a fast PC.

First start
  1. Unzip the folder anywhere you can write to (not Program Files).
  2. Start Takedawn.exe. It is not code-signed: if Windows SmartScreen says
     "Windows protected your PC", choose "More info", then "Run anyway".
  3. The setup window asks for your disc: a disc image (.iso / .xiso) or an
     extracted disc folder of Burnout 3: Takedown. It checks every file and
     copies the data into data\Game in this folder. Then press Play.
     Without the setup window, from a terminal:

         Takedawn.exe --install <image-or-folder>

  The European (PAL) discs work too, but as in Takedawn @UPVERSION@: two
  players. Only the North America disc's program has the four-player code.

Turning four players on
  Start the game once and quit. In data\Saves, open settings.toml in a
  text editor (Notepad is fine), or make it if it is not there yet, add
  these lines at the end and save:

      [splitscreen]
      players = 4

  Then choose Multiplayer > Race (or Road Rage) in the game. Four players
  pick their cars in turn. players = 2 (or no [splitscreen] lines) is the
  game as shipped. The other settings of that section, all optional:

      ai_players = "none"   # the game's AI drives "3", "4" or "34"
                            #   (players 3 and 4) when you are fewer
      keyboard_player = 1   # the keyboard drives player 1; 3 or 4: in
                            #   the race the arrow keys drive that player
      cars = "copy"         # players 3's and 4's cars until they pick one
      run_ahead = true      # draws a frame behind the game: about 60 fps
                            #   with four players instead of about 43

  The settings are read when the game starts.

Controllers
  Four controllers: each one becomes the next free player when its first
  button is pressed. At the title screen press a button on player 1's
  controller first, then on players 2's, 3's and 4's. The keyboard drives
  player 1 as well.

  Three controllers and the keyboard: make the keyboard player 1 and the
  controllers players 2 to 4 with these lines in settings.toml

      [input]
      first_pad = "player2"

  (or Settings > Controls > First controller: Player 2 in the game). In the
  race the arrow keys can drive player 3 or 4 instead (keyboard_player):
  Up accelerates, Down brakes, End or Period boosts.

Known limitations
  - The results screen lists players 1 and 2 only.
  - In Eliminator the race may end early or late when players 1 and 2 are
    out first: its rule was written for two players. Race counts all
    four; Road Rage has not been played to its end with four yet.
  - Props and textures stream around players 1 and 2 (the track itself
    follows all four), so players 3 and 4 may see fewer props far away.
  - All four share one set of speakers.

Where things are
  Everything stays in this folder (portable.txt turns that on):
  data\Game     the game data from your disc
  data\Saves    saves, settings.toml, logs
  data\cache    shader cache
  It never touches an installed Takedawn's saves (%APPDATA%\Takedawn). To
  use your saves from one, copy %APPDATA%\Takedawn into data\Saves first.

Updating
  Unzip the new version into a new folder and move your data folder into
  it (or copy the new files over the old folder; data\ is not in the zip).
  Settings > System > Check for updates asks github.com/leanbgit/takedawn.

The licences of everything inside are in the licenses folder;
licenses\SOURCE.txt says where the source of this build is.
