# Diceware

Generate diceware-style passwords using a strong RNG and a configurable word
list.

## Build

SQLite and libbsd are required to build on Linux. Once the dependencies are
installed, build using `scons`:

```
$ scons diceware
```

## Usage

To get started, initialize the database with a wordlist:

```
$ diceware -w eff_large_wordlist.txt
```

This will parse the word list and store it in a database at `~/.diceware.db`. To
use a different database, use `-d /path/to/database`.

To generate a passphrase with 4 words:

```
$ diceware -n 4
```

Note that it defaults to 4 words (about 50 bits of entropy), so the above could
instead just be:

```
$ diceware
```

