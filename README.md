# NetGreeting

A C++ / Qt 6 homage to Microsoft NetMeeting.

Place a call by IP, talk (audio + video or a bouncing test card), chat, sketch on a shared whiteboard, send files, and share your desktop. Two copies of the app on a LAN are enough — no H.323 stack, no account.

**Greeting Post** is the ILS stand-in: a tiny bulletin board of who is online. It never proxies a call.

## Build

```bash
sudo apt install build-essential cmake qt6-base-dev libpulse-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Installers

One script builds the installer for **this computer’s OS**:

```bash
./scripts/package.sh
```

| OS | What you get | How the user installs it |
|---|---|---|
| Ubuntu / Debian | `.deb` in `dist/` | `sudo apt install ./netgreeting_*.deb` |
| Fedora / RHEL | `.rpm` in `dist/` | `sudo dnf install ./netgreeting-*.rpm` |
| Windows | `.exe` installer | double-click the installer |
| Mac | `.dmg` | open it, drag NetGreeting to Applications |

Windows `.exe` and Mac `.dmg` have to be built **on** Windows and Mac (or by GitHub Actions: **Actions → Package**). You cannot produce those two from Linux.

## Run

```bash
./build/netgreeting
```

On the same machine, open a second window on another port:

```bash
./build/netgreeting --port 1721 --name Alice
./build/netgreeting --port 1720 --name Bob --call 127.0.0.1:1721
```

Then **Call → New Call** and dial `127.0.0.1` port `1721`.

### Greeting Post (directory)

Out of the box, NetGreeting logs you onto a shared public directory (the ILS stand-in). Open **Call → Directory**. Anyone else running the app appears in the list; select them and click **Call**.

You do not run a server. The directory is only a phone book — audio and video still go computer to computer.

Optional: run your own board with `./run.sh post` and point **Tools → Options → Server** at `host:1730`.

## What it does

| NetMeeting | NetGreeting |
|---|---|
| Direct IP call | TCP call on port 1720 (configurable) |
| Internet Locator Service | Greeting Post bulletin board + LAN nearby + SpeedDial |
| Chat (with whisper) | Chat (with whisper) |
| Whiteboard | Vector whiteboard + remote pointer |
| File transfer | Background file send/receive |
| Video / audio | V4L2 camera + PulseAudio, or a test card |
| Application sharing | View-only desktop share |
| H.323 / T.120 | Custom framed protocol (LAN-first) |

## Options

Set your name, e-mail, city, listen port, camera device (`/dev/video0`), and bandwidth (LAN / DSL / 56k) under **Tools → Options**. Bandwidth changes JPEG quality, frame size, and frame rate the way NetMeeting's connection speed setting did.

Received files are saved wherever you choose when you accept a transfer.

## Protocol

Length-prefixed TCP messages (`uint32` big-endian size + type byte + payload). Control messages are JSON; video, audio, and desktop share frames are JPEG or PCM. UDP multicast `239.255.17.20:1729` is used only to find neighbors.

This is hobby software for trusted networks. There is no encryption.

## Name

NetGreeting. Because a call should start with hello.
