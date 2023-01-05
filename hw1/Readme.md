# Build and run
`make all` produces the executable `shell` in the same directory. Run using `./shell` in the current directory.

# Built-in commands
The current shell version supports the following built-in commands:

| cmd  | function                                    |   
|:------:|:---------------------------------------------:|
| ?    | Show the help menu                          |  
| quit | Quit from the command shell                 |   
| pwd  | Print current working directory             |   
| cd   | Change directory                            |   
| wait | Wait for all background processes to finish |   

# Support
In addition to the built-ins, the shell supports the following:

* Running programs by providing their path or name. This path can be either absolute/relative path. In case the name is provided, the path should be found in the **PATH** environment variable.
* Input/output redirection using **>**. There should always be space around this operator. Currently, it does not support **>>**.
* Signals. The user can send standard Linux signals by pressing various combinations of **CTRL + KEY**
* Run in background. By putting **&** at the end of the command, the shell prompt returns immediately and the process starts at background. As mentioned before, **wait** can be used to wait for the completion of all background jobs.
