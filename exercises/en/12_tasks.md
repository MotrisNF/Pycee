# Exercise 12: a task list

| | |
|---|---|
| Files to hand in | `tasks.pce` and `storage.pce` |
| Uses | modules of your own (`import`, `from ... import ... as`), `io` (files and `eprint`), `os` (`exists`, `env`), `json`, `sys` (`exit`, `argv`) |
| In the manual | `man(modules)`, `man(import)`, `man(io)`, `man(io.read_file)`, `man(io.write_file)`, `man(io.eprint)`, `man(os.exists)`, `man(os)`, `man(json)`, `man(sys.exit)` |

A real program: it is used from the terminal, keeps its state in a file and is split into two modules.

## Statement

```
./pycee tasks.pce add TEXT...
./pycee tasks.pce list
./pycee tasks.pce done N
./pycee tasks.pce remove N
./pycee tasks.pce clear
```

The tasks are kept in `tasks.json`, in the current directory; when the environment variable `TASKS` exists, in the file it names. When the file does not exist, there are no tasks yet.

| Command | What it does | Writes |
|---|---|---|
| `add TEXT...` | Adds a task with the arguments joined by spaces and the next number | `added #3: write the report` |
| `list` | The tasks in order, and how many are done | `[x]  2. call Eva` per task (the number right-aligned in 2), and at the end `1/3 done`. With no tasks, `no tasks` before the total. |
| `done N` | Marks task `N` as done | `done #2: call Eva` |
| `remove N` | Removes task `N` | `removed #2: call Eva` |
| `clear` | Removes the tasks that are done | `removed K done tasks` |

- **Numbers are never reused**: remove task 2, and the next one added is 4, not 2.
- After every command that works, the file is saved.

### The file

JSON, in this shape (indented by 2 spaces, as `json.dump(data, indent = 2)` writes it, and a newline at the end):

```
{
  "next": 4,
  "tasks": [
    {
      "id": 1,
      "text": "buy bread",
      "done": false
    }
  ]
}
```

### Errors

Error messages go to the **error stream** (stderr), not to the normal output.

| Situation | Message | Code |
|---|---|---|
| no command, an unknown one, or the wrong number of arguments | `usage: tasks.pce add TEXT... \| list \| done N \| remove N \| clear` | 2 |
| `N` is not a number | `error: 'two' is not a task number` | 1 |
| no task has that number | `error: there is no task #9` | 1 |
| the file is not valid JSON, or not a dict with `next` and `tasks` | `error: bad.json is not a valid task file: <reason>` | 1 |

On an error nothing is saved.

### Design requirements

- `storage.pce` is a module with `load(path)` (gives back the data; when the file does not exist, empty data) and `save(path, data)`. A broken file raises an error of the module's own, `StorageError`.
- `tasks.pce` imports what it needs: `from storage import load, save as store, StorageError;`. Reading, parsing and writing the file belong to `storage.pce`; the messages and the exit codes, to `tasks.pce`.

## Example

```
$ ./pycee tasks.pce list
no tasks
0/0 done
$ ./pycee tasks.pce add buy bread
added #1: buy bread
$ ./pycee tasks.pce add call Eva
added #2: call Eva
$ ./pycee tasks.pce add "write the report"
added #3: write the report
$ ./pycee tasks.pce done 2
done #2: call Eva
$ ./pycee tasks.pce list
[ ]  1. buy bread
[x]  2. call Eva
[ ]  3. write the report
1/3 done
$ ./pycee tasks.pce done 9; echo "exit=$?"
error: there is no task #9
exit=1
$ ./pycee tasks.pce clear
removed 1 done tasks
$ ./pycee tasks.pce list
[ ]  1. buy bread
[ ]  3. write the report
0/2 done
$ echo '{"next": 1' > bad.json
$ TASKS=bad.json ./pycee tasks.pce list; echo "exit=$?"
error: bad.json is not a valid task file: json.parse(): expected ',' or '}' but the text ended at line 2, column 1
exit=1
```

To check that errors go to stderr: `./pycee tasks.pce done 9 2> /dev/null` must write nothing.

## Going further (optional)

- Dates: keep when each task was made (`man(time)`) and show it in `list`.
- `undo`: undoes the last command (keep the previous version of the file).
