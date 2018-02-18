# Docker Images

## Suggested Usage

- **Build the docker image.**

```shell
cd LKH
docker build -t lkh .
```

- **Source the alias file to make life easier.**

```shell
source aliases
```

- **Run `lkh-six` to begin the equivalent of `bin/lkh_runner.py -o lkh/out/1/`.**

```shell
lkh-six $PWD/lkh-six-out/1
```

You can run as many of these as you like; just change the output directory to avoid conflicts. Hit <kbd>Ctrl</kbd>+<kbd>C</kbd> to stop the process.
