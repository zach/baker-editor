# 0013: The console answers many requests in one frame without delay

## The need

When a tool sends many requests to the remote console at once (one line each, in request
form), the console answers them all in the frame that reads them, and the replies reach
the tool in that frame too: the time for 100 requests is about one frame, as for one.

Today the console answers them all in one poll, but sends each reply as its own small
write. With the operating system's default for TCP (Nagle's algorithm) and a client that
delays its acknowledgements, as Linux does by default, each small write after the first
waits for an acknowledgement that is held back about 40 ms. Sending each poll's replies
in one write, or setting `TCP_NODELAY` on the console's sockets, ends the wait.

## Why

A tool showing live values beside the data (an editor's inline hints, a dashboard, an
agent comparing the running game with the files) asks about every value on screen at once.
With a 50 ms budget per batch, the delay makes the batch miss its budget about half the
time, and the tool shows nothing or stalls.

## Constraints

- No cost to a game without a client: the change is in how replies are written.
- The reply format and order are unchanged.
- Measured on Linux against the Duel stack (headless, 60 Hz): 60 pipelined
  `config.explain` requests took 48–60 ms; with the client forcing immediate
  acknowledgements, 12–17 ms, and 200 requests took the same.

## Today

The editor's server asks the operating system to acknowledge at once before each read
(Linux only), which brings a batch of 113 requests to 18–22 ms. Other platforms, and
other tools, still wait.

## Not included

This note contains no code, identifiers, structure or documentation from any other
product, and nothing confidential.
