# Introduction
- Launches an application with a set of custom environment variables
- These environment variables correspond to a set of  directories which emulate window's directory structure
- Useful for containerising game or application save files

# Preview
![Main window](docs/screenshot_v1.png)

# Additional Notes
Unfortunately some games read the Windows registry to get their environment variables which we cannot modify. 

Refer to [print_environment.c](src/print_environment.c) for a program that reads it in an interceptable way for our virtual environment program.

If you encounter a game which reads it from the Windows registry, the best alternative is to create a symbolic link to store the game files in a portable location.

The following windows command will create a symbolic link into a virtual environment. 

<code>mklink /J "%userprofile%/[our_game_directory]" "./envs/[our_env]/Users/[our_username]/[our_game_directory]"</code>. 

**NOTE**: Games may store their save and configuration files in other locations. <code>%userprofile%</code> or a subdirectory is usually the most common location.

- <code>%userprofile%/[our_game_directory]</code>
- <code>%userprofile%/Documents/[our_game_directory]</code>
- <code>%userprofile%/Documents/My Games/[our_game_directory]</code>
- <code>%userprofile%/Saved Games/[our_game_directory]</code>
- <code>%userprofile%/AppData/Roaming/[our_game_directory]</code>

This is not a definitive list and you may have to do your own research. 