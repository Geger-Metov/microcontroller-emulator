# Project documentation

Short notes — "for myself in six months", and for new contributors. Read them in this order:

1. [architecture.md](architecture.md) — what the project consists of and how data flows from a HEX file to an LED.
2. [how-to.md](how-to.md) — recipes: build, add an instruction/register/peripheral/test.
3. [status.md](status.md) — what is done, what is not, limitations and next steps.
4. [decisions.md](decisions.md) — why things are done this way (decision log).
5. [glossary.md](glossary.md) — AVR conventions that are easy to forget (words vs bytes, +0x20, etc.).
6. [references.md](references.md) — the Microchip documents the implementation follows, and where to get them.

Rule: **if you change behavior, update the document in the same commit.** Less text, but correct, is better.
