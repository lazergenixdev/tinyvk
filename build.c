#define NOB_IMPLEMENTATION
#include "nob.h"

#ifdef _WIN32
#	define EXT ".exe"
#	define DIR '\\'
#else
#	define EXT ""
#	define DIR '/'
#endif

Cmd cmd;
const char* vulkan_sdk_path;
const char* slangc;

bool setup_vulkan(const char* output_dir)
{
	vulkan_sdk_path = getenv("VULKAN_SDK");
	assert(vulkan_sdk_path && "Ensure `VULKAN_SDK` environment variable points to your local Vulkan SDK!");
	slangc = temp_sprintf("%s/bin/slangc", vulkan_sdk_path);
#if defined(_WIN32)
	return true;
#elif defined(__APPLE__)
    // NOTE: Application required this shared library to run
	const char* source = temp_sprintf("%s/lib/libvulkan.1.dylib", vulkan_sdk_path);
	const char* destination = temp_sprintf("%s/libvulkan.1.dylib", output_dir);
	return copy_file(source, destination);
#elif defined(__linux__)
	return true;
#endif
}

void add_vulkan(Cmd *cmd)
{
#if defined(_WIN32)
	cmd_append(cmd, "-I", temp_sprintf("%s/Include", vulkan_sdk_path));
	cmd_append(cmd, "-L", temp_sprintf("%s/Lib", vulkan_sdk_path));
#elif defined(__APPLE__)
	cmd_append(cmd, "-I", temp_sprintf("%s/include", vulkan_sdk_path));
	cmd_append(cmd, "-L", temp_sprintf("%s/lib", vulkan_sdk_path));
#elif defined(__linux__)
	// TODO
#endif
}

bool build_example(Walk_Entry entry)
{
	if (entry.level == 0 || entry.type != NOB_FILE_DIRECTORY)
		return true;
	
	// Skip all files inside example directories
	if (entry.level > 0) {
		*entry.action = WALK_SKIP;
	}

	String_View name = sv_from_cstr(entry.path);
	sv_chop_by_delim(&name, DIR);

	// Compile shaders

	File_Paths children = {};
	assert(read_entire_dir(entry.path, &children));
	for (int i = 0; i < children.count; ++i)
	{
		String_View filename = sv_from_cstr(children.items[i]);
		if (!sv_ends_with_cstr(filename, ".slang"))
			continue;

		const char* shader_path = temp_sprintf("%s/%s", entry.path, children.items[i]);

		cmd_append(&cmd, slangc);
		cmd_append(&cmd, "-target", "spirv");
		cmd_append(&cmd, "-source-embed-style", "u32");
		cmd_append(&cmd, shader_path);
		cmd_append(&cmd, "-o", shader_path);

		if (!cmd_run(&cmd))
			return false;
	}

	// Compile Application

	cmd_append(&cmd, "clang", "-std=c99");
	cmd_append(&cmd, "-Wall", "-Wextra"); // Extra warnings
	cmd_append(&cmd, "-g"); // Debug flags
	cmd_append(&cmd, "-I.");
#if defined(_WIN32)
	cmd_append(&cmd, "-Wno-deprecated-declarations");
	cmd_append(&cmd, "-lgdi32", "-lvulkan-1");
#elif defined(__APPLE__)
	cmd_append(&cmd, "-framework", "Cocoa", "-framework", "IOKit", "-lvulkan");
	cmd_append(&cmd, "-Wl,-rpath,@executable_path");
#elif defined(__linux__)
	cmd_append(&cmd, "-lX11", "-lXrandr", "-lvulkan");
#endif
	add_vulkan(&cmd);
	cmd_append(&cmd, temp_sprintf("%s/main.c", entry.path));
	cmd_append(&cmd, "-o", temp_sprintf("bin/%.*s"EXT, (int)(name.count), name.data));
	
	if (!cmd_run(&cmd))
		return false;

	return true;
}

int main(int argc, char* argv[])
{
	GO_REBUILD_URSELF(argc, argv);

	if (!mkdir_if_not_exists("bin"))
		return 1;

	if (!setup_vulkan("bin"))
		return 1;

	if (!walk_dir("examples", build_example))
		return 1;

	return 0;
}
