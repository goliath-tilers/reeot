> [!CAUTION]
> This project is in early development. It currently does not display a screen, due to a number of issues, but the dev team of the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) are working on making fixes, and hopefully we'll be able to see more progress soon :D

# EdgeOfTime-Recompiled

EdgeOfTime-Recompiled (codenamed "reeot") is an unofficial PC port of the Xbox 360 version of "Spider-Man: Edge Of Time" created through the process of static recompilation. The port offers Windows, macOS, and Linux support with goals for numerous built-in enhancements such as high resolutions, ultrawide support, high frame rates, improved performance and modding.

***This project does not include any game assets. You must provide the files from your own legally acquired copy of the game to install or build EdgeOfTime-Recompiled.***

EdgeOfTime-Recompiled uses the ReXGlue SDK to convert PowerPC Assembly to static C++ code that can be compiled to any platform, with a custom XenosRecomp fork to convert Xbox 360 Shaders from the compiled shader container to static HLSL code. Given that the system is currently in development, there are no recommended or minimum settings designed at the moment, and no playtesters to confirm or deny any settings. As mentioned in rules, ***THERE IS NO OFFICIAL RELEASE AS OF JULY 2026, DO NOT TRUST ANYONE CLAIMING THEY HAVE A PC PORT***

# TODO

## Situation: Repository is DNI
Currently, this README and a majority of this repository is incomplete. The repository will give you enough information to be able to statically recompile the `default.xex` (with TU) and `GameLogic.dll` binaries of the title Spider-Man Edge of Time. Due to significant restrictions in how the emulated rendering system of ReXGlue works, we CANNOT use it.


https://github.com/user-attachments/assets/87c2a478-b94e-43d0-8f5f-5c23e10ffd2e

## Solution: Rendering replacement
Due to this significant constraint, the development team of EdgeOfTime-Recompiled has had to make a custom "native" rendering solution behind closed doors. This rendering solution replaces the fundamentals set in Xenia, and hooks directly onto the executable's embedded D3DX9 library to execute rendering calls. There is currently a private repository that [Graine25](https://github.com/Graine25) hosts away from the public. Eventually, when he is finished with updating that repository, he will send out builds to play testers and many alike to review. As some proof of existence, here is some pre-release footage:


https://github.com/user-attachments/assets/95bbe327-8363-4dbc-96ef-e0079e6e0a73


## Contact

[Recomp Discord Server](https://discord.gg/s9JwT3kHZd) - Easiest way to connect with the development team of this project


# Credits

Huge thanks to everyone who's put time into this. EdgeOfTime-Recompiled wouldn't be where it is without you.

* **[Graine25](https://github.com/Graine25)**: Creator of EdgeOfTime-Recompiled and maintainer of the ReXGlue SDK.
* **[Serjar](https://www.youtube.com/channel/UCaCoblwXlhhZFoJVPc8L2cg)**: one of the few people outside of the original beenox dev team who knows EdgeOfTime like the back of their hand. A lot of the reversing, between understanding the PAK format and how it interacts in the game code, would not have been possible without his help.
* The **[ReXGlue SDK](https://github.com/rexglue/rexglue-sdk)** team, for the toolchain this project is built on.
* **[UnleashedRecompiled](https://github.com/hedge-dev/unleashedrecomp/)** for setting the bar on how incredible a Static Recompilation can be, and proving to a wider audience that 360 titles can be relived on modern computers. 
* The wider **Xbox 360 emulation scene**. A lot of the hardest problems were solved by them long before this project started.

## License

See [LICENSE](LICENSE).


