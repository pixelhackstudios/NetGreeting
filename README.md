# NetGreeting

A simple video-call app, like Microsoft NetMeeting used to be.

You can see who’s online, call them, talk, chat, draw on a shared whiteboard, send files, and share your screen. There is no account to create. Open the app, type your name, and you’re in.

## Get it

Download the installer for your computer:

**[Download NetGreeting](https://github.com/pixelhackstudios/NetGreeting/releases/latest)**

| Your computer | Which file | What to do |
|---|---|---|
| Windows | the `.exe` file | Double-click it and follow the steps |
| Mac | the `.dmg` file | Open it, then drag NetGreeting into Applications |
| Ubuntu / Debian | the `.deb` file | Open a terminal in that folder and run `sudo apt install ./netgreeting_*.deb` |
| Fedora | the `.rpm` file | Open a terminal in that folder and run `sudo dnf install ./netgreeting-*.rpm` |

## How to use it

1. Open NetGreeting.
2. Go to **Tools → Options** and type your real first name. Click OK.
3. Go to **Call → Directory**.
4. If someone else has the app open, their name is in the list. Click it, then click **Call**.
5. When someone calls you, click **Accept**.

That’s the whole idea. The directory is a shared phone book of people who have the app open right now. You don’t run a server, and you don’t sign up for anything.

During a call you can:

- See each other on camera (or a test pattern if the camera isn’t available)
- Talk
- Open **Chat**
- Open **Whiteboard** and draw together
- Send a file
- Share your desktop (view only)

## A couple of honest notes

- Both people need the app open at the same time.
- The directory only answers “who’s online?” The actual call goes from your computer to theirs. If one of you is on a home network that blocks incoming calls, you’ll see each other in the list but the call may not connect. Same as NetMeeting on a home modem.
- This is a hobby project. Calls are not encrypted. Use it with people you trust, on a network you trust.

## For people who like to tinker

On Linux you can also run it from this folder:

```bash
./run.sh
```

To build installers on the machine in front of you:

```bash
./scripts/package.sh
```

Windows and Mac installers are built automatically on GitHub (Actions → Package).

NetGreeting. Because a call should start with hello.
