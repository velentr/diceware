import commands

host_os = commands.getoutput('uname -s').lower()

env = Environment(
        CCFLAGS = '-Wall -Wextra -Werror -std=c99 -pedantic -O2 -march=native'
      )

env.MergeFlags('!pkg-config --libs --cflags sqlite3')

# If we are on linux, we also need to link in libbsd.
if host_os.__contains__('linux'):
    env.MergeFlags('!pkg-config --libs --cflags libbsd')

env.Program('diceware', ['main.c', 'diceware.c'])

