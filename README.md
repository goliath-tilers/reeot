> [!CAUTION]
> This project is in early development. It currently does not display a screen, due to a number of issues, but the dev team of the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) are working on making fixes, and hopefully we'll be able to see more progress soon :D

<img width="1100" height="330" alt="Comp_1_0-00-00-00" src="https://github.com/user-attachments/assets/e4bca27a-b84f-463b-bb25-52c8c78fb4c5" />

# EdgeOfTime-Recompiled

EdgeOfTime-Recompiled (codenamed "reeot") is an unofficial PC port of the Xbox 360 version of "Spider-Man: Edge Of Time" created through the process of static recompilation. The port offers Windows, macOS, and Linux support with goals for numerous built-in enhancements such as high resolutions, ultrawide support, high frame rates, improved performance and modding.

***This project does not include any game assets. You must provide the files from your own legally acquired copy of the game to install or build EdgeOfTime-Recompiled.***

EdgeOfTime-Recompiled uses the ReXGlue SDK to convert PowerPC Assembly to static C++ code that can be compiled to any platform, with a custom XenosRecomp fork to convert Xbox 360 Shaders from the compiled shader container to static HLSL code. Given that the system is currently in development, there are no recommended or minimum settings designed at the moment, and no playtesters to confirm or deny any settings. As mentioned in rules, ***THERE IS NO OFFICIAL RELEASE AS OF JULY 2026, DO NOT TRUST ANYONE CLAIMING THEY HAVE A PC PORT***

## Contact

[Recomp Discord Server](https://discord.gg/PsReBEDDZX) - Easiest way to connect with the development team of this project

# TODO

## Situation: Repository is DNI
Currently, this README and a majority of this repository is incomplete. The repository will give you enough information to be able to statically recompile the `default.xex` (with TU) and `GameLogic.dll` binaries of the title Spider-Man Edge of Time. Due to significant restrictions in how the emulated rendering system of ReXGlue works, we CANNOT use it.


https://github.com/user-attachments/assets/87c2a478-b94e-43d0-8f5f-5c23e10ffd2e

## Solution: Rendering replacement
Due to this significant constraint, the development team of EdgeOfTime-Recompiled has had to make a custom "native" rendering solution behind closed doors. This rendering solution replaces the fundamentals set in Xenia, and hooks directly onto the executable's embedded D3DX9 library to execute rendering calls. There is currently a private repository that [Graine25](https://github.com/Graine25) hosts away from the public. Eventually, when he is finished with updating that repository, he will send out builds to play testers and many alike to review. As some proof of existence, here is some pre-release footage:


https://github.com/user-attachments/assets/95bbe327-8363-4dbc-96ef-e0079e6e0a73


# Contributions
### What we need
At the moment, contributions to this repository are paused, given the fact that the project is NOT in a releasable state. If you wish to contribute to reeot, we desperately need people who have experience in Graphics Programming and Graphics debugging, as well as people who are experienced in reversing the game's PowerPC assembly. RenderDoc and IDA Pro are a must.

### AI Usage
Whilst the codegen, ReXGlue SDK, and overall current setup/mods were made by hand, I would not have been able to get my rendering implementation without the help of many talented individuals at the ReXGlue team. I have no problems with using AI for this project, but I do **require that everyone who contributes has a strong understanding of what they're getting into**. This repository has been setup in a way that will not allow individuals to AI generate nonsense and push it upstream. I do not plan on vibecoding this entire system as I am very passionate about the game, and I want others to enjoy it as much as I have and be able to use the knowledge learnt from reversing on other titles with a similar setup. 

Do **NOT** make pull requests spearheading with an LLM, or spam issues with information provided by one of the many providers. I would prefer to see human reports as to what is going wrong, and how you've approached the sitation. I plan on detailing the process of how to build the system and how to use it/report issues properly. I appreciate everyone who's been along for the ride, and wish to continue building this piece-by-piece :)

# Credits

Huge thanks to everyone who's put time into this. EdgeOfTime-Recompiled wouldn't be where it is without you.

* **[Graine25](https://github.com/Graine25)**: Creator of EdgeOfTime-Recompiled and maintainer of the ReXGlue SDK.
* **[Serjar](https://www.youtube.com/channel/UCaCoblwXlhhZFoJVPc8L2cg)**: one of the few people outside of the original beenox dev team who knows EdgeOfTime like the back of their hand. A lot of the reversing, between understanding the PAK format and how it interacts in the game code, would not have been possible without his help.
* The **[ReXGlue SDK](https://github.com/rexglue/rexglue-sdk)** team, for the toolchain this project is built on.
* **[UnleashedRecompiled](https://github.com/hedge-dev/unleashedrecomp/)** for setting the bar on how incredible a Static Recompilation can be, and proving to a wider audience that 360 titles can be relived on modern computers. 
* The wider **Xbox 360 emulation scene**. A lot of the hardest problems were solved by them long before this project started.

## License

See [LICENSE](LICENSE).


