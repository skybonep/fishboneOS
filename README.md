# fishboneOS

**fishboneOS** is a simple system made of small, separate parts. You can pick the pieces you like and put them together to build your own custom system. Every piece can be changed or replaced easily. It is built for user choice and simple connections. We are starting with a small base to grow a project that is easy to use.

## Project Goals
The core vision of fishboneOS is to provide an ultra-modular operating system "playground" for developers. Unlike traditional systems with fixed cores, this project treats every component—including the kernel—as a replaceable module.

- Extreme Modularity: Inspired by projects like LegoOS and CharlotteOS, the system splits traditional OS functionalities into loosely-coupled monitors or modules. Users can select their preferred modules at installation-time to create a custom build. Furthermore, the system aims to allow the replacement of modules at run-time, providing a dynamic environment for testing new drivers or managers.

- User-Driven Customization: Users should be able to select specific modules—such as different filesystems, schedulers, or drivers—to assemble an OS tailored to their specific hardware or task requirements.

- A "No Must-Have" Philosophy: In this architecture, there are no "hard" dependencies. Even the kernel is treated as a module, allowing a developer to swap out a monolithic kernel for a microkernel or a specialized research manager depending on their needs.

- Developer-First Design: The initial target audience is developers and researchers. Because these users possess the technical knowledge to understand specific module functions, they can better navigate the choices required to assemble a functional system.

- Stability through Modular Isolation: To ensure that a "bad" or experimental module does not "nuke" the entire system, fishboneOS will implement graceful failure mechanisms
. This isolation ensures that if one module fails, the rest of the system remains retrievable and stable.

- Simplicity over Performance: The codebase will prioritize clean, readable C and Assembly. The goal is a system that is easy to audit and modify, ensuring it can be used and understood by other people in the OSDev community.


