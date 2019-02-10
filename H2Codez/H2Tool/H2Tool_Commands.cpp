#include "ToolCommandDefinitions.inl"
#include "H2Tool_extra_commands.inl"
#include "Tags\ScenarioTag.h"
#include "Common\H2EKCommon.h"
#include "util\Patches.h"
#include "util\string_util.h"
#include "util\numerical.h"
#include "Version.h"
#include "stdafx.h"
#include <regex>

using namespace HaloScriptCommon;

//I should better mention  the H2tool version i am using
//tool  debug pc 11081.07.04.30.0934.main  Apr 30 2007 09:37:08
//Credits to Kornmann https://bitbucket.org/KornnerStudios/opensauce-release/wiki/Home


static const s_tool_command* h2tool_extra_commands[] = {
	// commands in tool not added to the command table
	CAST_PTR(s_tool_command*, 0x97B4C8), // structure plane debug generate
	CAST_PTR(s_tool_command*, 0x97B4DC), // structure plane debug
	CAST_PTR(s_tool_command*, 0x97B56C), // bitmaps with type
	//
	&import_model_render,
	&import_model_collision,
	&import_model_physics,
	&import_model,
	&import_model_animations,
	import_class_monitor_structures,
	import_class_monitor_bitmaps,
	&import_sound,
	&tool_build_structure_from_jms,
	&h2dev_extra_commands_defination,
	&list_extra_commands,
	&copy_pathfinding,
	&pathfinding_from_coll
};

int __cdecl s_tool_command_compare(void *, const void* lhs, const void* rhs)
{
	const s_tool_command* _lhs = *CAST_PTR(const s_tool_command* const*, lhs);
	const s_tool_command* _rhs = *CAST_PTR(const s_tool_command* const*, rhs);

	const wchar_t* l = _lhs->name;
	const wchar_t* r = _rhs->name;
	if (l[0] == L'~') l++;
	if (r[0] == L'~') r++;

	return _wcsicmp(l, r);
}

#pragma region structure_import Tweeks
void H2ToolPatches::Increase_structure_import_size_Check()
{
	//if ( FileSize.LowPart > 0x1400000 && !FileSize.HighPart )
	 ///1400000h =  20971520 BYTES ~ 20 MB

/*
.text:0041F836 C90 cmp     dword ptr [esp+0C90h+FileSize], 1400000h ; Compare Two Operands <-Change This Size
.text:0041F83E C90 jbe     loc_41F8D9                      ; Jump if Below or Equal (CF=1 | ZF=1)
.text:0041F83E
.text:0041F844 C90 cmp     dword ptr [esp+0C90h+FileSize+4], ebx ; Compare Two Operands
.text:0041F848 C90 jnz     loc_41F8D9 
*/


	//increasing the 20 MB File Import Check
	DWORD new_size = 0x1400000 * TOOL_INCREASE_FACTOR; ///671.08864 MB
	WriteValue(0x41F836 + 4, new_size);

	// push <max file size>
	// TODO: check this
	// WriteValue(0x589183 + 1, new_size);
}
void H2ToolPatches::structure_bsp_geometry_collision_check_increase()
{
//return collision_surfaces_count <= 0x7FFF && edges_count <= 0xFFFF && collision_vertices_count <= 0xFFFF;
	///0x7FFF = 32767
	///0xFFFF = 65535
/*
.text:005A2D50 000 cmp     [esp+collision_surfaces_count], 7FFFh ; Compare Two Operands
.text:005A2D58 000 jg      short loc_5A2D6E                ; Jump if Greater (ZF=0 & SF=OF)
.text:005A2D58
.text:005A2D5A 000 mov     eax, 0FFFFh
.text:005A2D5F 000 cmp     [esp+a2], eax 
*/
   //increasing the collision_surfaces_count
	DWORD collision_surfaces_count = 0x7FFF * TOOL_INCREASE_FACTOR; /// 0xFFFE0
	WriteValue(0x5A2D50 + 4, collision_surfaces_count);


	//increasing the edges_vertices_count
	DWORD edges_vertices_count = 0xFFFF * TOOL_INCREASE_FACTOR; /// 0x1FFFE0
	WriteValue(0x5A2D5A + 1, edges_vertices_count);

	///Also Patching in the error_proc method incase we ever hit this Limit :)
/*
.text:00464C50 118 push    0FFFFh
.text:00464C55 11C push    eax
.text:00464C56 120 push    0FFFFh
.text:00464C5B 124 push    ecx
.text:00464C5C 128 push    7FFFh
*/
	WriteValue(0x464C50 + 1, edges_vertices_count);
	WriteValue(0x464C56 + 1, edges_vertices_count);
	WriteValue(0x464C5C + 1, collision_surfaces_count);


}
void H2ToolPatches::structure_bsp_geometry_3D_check_increase()
{
	// return nodes_count < &unk_800000 && planes_count < 0x8000 && leaves_count < &unk_800000;
	/// planes_count =0x8000 = 32768 
/*
.text:005A2CFB 000 cmp     [esp+planes_count], 32768       ; Compare Two Operands
.text:005A2D03 000 jge     short loc_5A2D0E                ; Jump if Greater or Equal (SF=OF)
.text:005A2D03
*/
	////increasing the planes_count Check
	DWORD new_planes_count = 0x8000 *TOOL_INCREASE_FACTOR; /// 0x100000
	WriteValue(0x5A2CFB + 4, new_planes_count);

}
void H2ToolPatches::structure_bsp_geometry_2D_check_increase()
{
/*
.text:00464BA5 118 push    0        <- b_DONT_CHECK variable is set to false by default .Need to make it true
.text:00464BA7 11C push    ecx
.text:00464BA8 120 push    edx
.text:00464BA9 124 call    collision_bsp_2d_vertex_check   ; Call Procedure
*/
	WriteBytes(0x464BA5 + 1, new BYTE{1}, 1);

	//No Need to modify the Proc error here cuz it will never hit :)
}

static const int BSP_MAX_DEPTH = 512;

static const int function_epilog = 0x00465797;
static void __declspec(naked) bsp_depth_check()
{
	__asm {
		add eax, 1
		cmp eax, BSP_MAX_DEPTH
		jle continue_compile
		jmp abort_with_error

		continue_compile:
		// remove return address
		pop eax
	    // push function epilog for ret
		push function_epilog
		ret


		abort_with_error:
		ret
	}
}

void structure_bsp_depth_check_increase()
{
	// remove old check
	NopFill(0x00465726, 0xA);

	// write call to our check
	WriteCall(0x00465726, bsp_depth_check);
}

void H2ToolPatches::Increase_structure_bsp_geometry_check()
{
	H2PCTool.WriteLog("Increasing structure_bsp_geometry checks");
	structure_bsp_geometry_2D_check_increase();
	structure_bsp_geometry_3D_check_increase();
	structure_bsp_geometry_collision_check_increase();
	structure_bsp_depth_check_increase();
}

#pragma endregion

#pragma region Shared_tag_removal_scheme
//A reference to H2EK_OS tools SansSharing.inl
/*
* shared tag scheme removal changes

.text:005887DF                 push    1               ; CHANGE THIS TO FALSE
.text:005887E1                 mov     esi, eax
.text:005887E3                 call    _postprocess_animation_data

.text:00588C21                 push    1               ; CHANGE THIS TO FALSE
.text:00588C23                 push    edx
.text:00588C24                 mov     esi, eax
.text:00588C26                 call    _build_cache_file_prepare_sound_gestalt

.text:00588D52                 push    1               ; CHANGE THIS TO FALSE
.text:00588D54                 push    0
.text:00588D56                 push    0
.text:00588D58                 push    0
.text:00588D5A                 lea     edx, [esp+1F94h+multiplayer_shared_dependency_graph]
.text:00588D61                 mov     esi, eax
.text:00588D63                 push    edx
.text:00588D64                 lea     eax, [esp+1F98h+cache_header]
.text:00588D6B                 push    eax
.text:00588D6C                 lea     edx, [esp+1F9Ch+scenario_name_wide]
.text:00588D70                 call    _build_cache_file_add_tags ; ecx = s_shared_tag_index

.text:00588DCD                 push    1               ; CHANGE THIS TO FALSE
.text:00588DCF                 mov     ecx, edi
.text:00588DD1                 mov     esi, eax
.text:00588DD3                 call    _build_cache_file_add_sound_samples

.text:00588E37                 push    1               ; CHANGE THIS TO FALSE
.text:00588E39                 push    ecx
.text:00588E3A                 mov     esi, eax
.text:00588E3C                 call    _build_cache_file_sound_gestalt

.text:00588E99                 push    1               ; CHANGE THIS TO FALSE
.text:00588E9B                 lea     edx, [esp+1F88h+cache_header]
.text:00588EA2                 push    edx
.text:00588EA3                 push    edi
.text:00588EA4                 mov     esi, eax
.text:00588EA6                 call    _build_cache_file_add_geometry_blocks

.text:0058908E                 push    1               ; CHANGE THIS TO FALSE
.text:00589090                 mov     esi, eax
.text:00589092                 call    _language_packing

.text:005890A7                 push    1               ; CHANGE THIS TO FALSE
.text:005890A9                 push    ecx
.text:005890AA                 call    _build_cache_file_add_language_packs

.text:00589105                 push    1               ; CHANGE THIS TO FALSE
.text:00589107                 push    edi
.text:00589108                 mov     esi, eax
.text:0058910A                 call    _build_cache_file_add_bitmap_pixels

.text:00589181                 push    1               ; CHANGE THIS TO FALSE
.text:00589183                 push    1400000h        ; base address
.text:00589188                 push    edx
.text:00589189                 mov     esi, eax
.text:0058918B                 push    edi
.text:0058918C                 lea     eax, [esp+1F94h+multiplayer_shared_dependency_graph]
.text:00589193                 push    eax
.text:00589194                 lea     ecx, [esp+1F98h+cache_header]
.text:0058919B                 push    ecx
.text:0058919C                 mov     ecx, [esp+1F9Ch+multiplayer_shared_tag_index_sorted]
.text:005891A0                 lea     edx, [esp+1F9Ch+scenario_name_wide]
.text:005891A4                 call    _build_cache_file_add_tags ; ecx = s_shared_tag_index

*/

#pragma endregion

void H2ToolPatches::apply_shared_tag_removal_scheme()
{
	DWORD patching_offsets_list[] = {
		0x5887DF,	// _postprocess_animation_data
		0x588C21,	// _build_cache_file_prepare_sound_gestalt
		0x588D52,	// _build_cache_file_add_tags
		0x588DCD,	// _build_cache_file_add_sound_samples
		0x588E37,	// _build_cache_file_sound_gestalt
		0x588E99,	// _build_cache_file_add_geometry_blocks
		0x58908E,	// _language_packing
		0x5890A7,	// _build_cache_file_add_language_packs
		0x589105,	// _build_cache_file_add_bitmap_pixels
		0x589181	// _build_cache_file_add_tags
	};

	for (int x = 0; x < NUMBEROF(patching_offsets_list); x++)
		WriteValue<BYTE>(patching_offsets_list[x] + 1, 0);
}

void H2ToolPatches::unlock_other_scenario_types_compiling()
{
	//Refer to H2EK_OpenSauce Campaign_sharing
	static void* BUILD_CACHE_FILE_FOR_SCENARIO__CHECK_SCENARIO_TYPE = CAST_PTR(void*,0x588320);
	BYTE patch[1] = {0xEB};
	WriteBytes((DWORD)BUILD_CACHE_FILE_FOR_SCENARIO__CHECK_SCENARIO_TYPE, patch, 1);//change jz to jmp

}

static signed long _scenario_type;

static void __declspec(naked) _build_cache_file_for_scenario__intercept_get_scenario_type()
{
	//Refer to H2EK_OpenSauce Campaign_sharing
	//Basically this function helps us to store the scenario_type which can be used in later areas
	static const unsigned __int32 INTERCEPTOR_EXIT = 0x588313;

	__asm {
		    movsx	edx, word ptr[eax + 0x1C]
			push	esi
			mov[esp + 0x58], edx			// mov     [esp+1F90h+scenario_type], edx
			mov		_scenario_type, edx
			jmp		INTERCEPTOR_EXIT
	}
}

const static wchar_t *campaign_shared_path = L"maps\\single_player_shared.map";
const static char *load_sharing_log_messages[] =
{
	"tag sharing: loading tag names from single_player_shared.map",
	"tag sharing: failed to open singleplayer shared map file",
	"singleplayer shared cache file doesn't have its string table!! AAAAaaaaggghh!!!",
	"singleplayer shared cache file string count is suspect (> 0x6000)",
	"singleplayer shared cache file doesn't have its tag dependency graph!! AAAAaaaaggghh!!!",
	"singleplayer shared cache file tag dependency graph size is suspect (> 0x100000)",
	"tag sharing: ruh roh, single_player_shared.map has no shared tags"

};

const static int load_sharing_log_offsets[] =
{
	0x005813E7,
	0x00581499,
	0x005814B7,
	0x005818FF,
	0x00581587,
	0x005818EB,
	0x005817CD
};

static char __cdecl h_BUILD_CACHE_FILE_FOR_SCENARIO__TAG_SHARING_LOAD_SHARED(void *a1, void* a2)
{
	//Refer to H2EK_OpenSauce Campaign_sharing

	// If scenario_type is single_player,modify the strings
	if (_scenario_type == 0)
	{	
		// Replace file name strings
		WritePointer(0x581455, campaign_shared_path);
		WritePointer(0x581480, campaign_shared_path);

		// fix log strings to match
		for (int i = 0; i < ARRAYSIZE(load_sharing_log_offsets); i++)
			WritePointer(load_sharing_log_offsets[i] + 1, load_sharing_log_messages[i]);
	}
	H2PCTool.WriteLog("loading....tag_sharing method");


	DWORD TAG_SHARING_LOAD_MULTIPLAYER_SHARED = 0x5813C0;
	return ((char(__cdecl *)(void*, void*))TAG_SHARING_LOAD_MULTIPLAYER_SHARED)(a1,a2);// call Function via address

}
void H2ToolPatches::render_model_import_unlock()
{
	//Patches the h2tool to use the custom render_model_generation methods

	///Patch Details::#1 patching the orignal render_model_generate function to call mine
	/*
	.text:0041C7A0 000                 mov     eax, [esp+arguments]
	.text:0041C7A4 000                 mov     ecx, [eax]
	.text:0041C7A6 000                 jmp     loc_41C4A0      ; Jump
	*/
	PatchCall(0x41C7A6, h2pc_import_render_model_proc);
}

void H2ToolPatches::enable_campaign_tags_sharing()
{
	//Refer to H2EK_OpenSauce Campaign_sharing

    void* BUILD_CACHE_FILE_FOR_SCENARIO__GET_SCENARIO_TYPE = CAST_PTR(void*, 0x58830A);
	int BUILD_CACHE_FILE_FOR_SCENARIO__TAG_SHARING_LOAD_SHARED =  0x5883DE;

	/*
	//get_scenario_ Intercept codes
	BYTE patch[1] = { 0xE8};

	//Writing a call in memory to jmp to our function
	WriteBytesASM((DWORD)BUILD_CACHE_FILE_FOR_SCENARIO__GET_SCENARIO_TYPE, patch, 1);
	PatchCall((DWORD)BUILD_CACHE_FILE_FOR_SCENARIO__GET_SCENARIO_TYPE, (DWORD)_build_cache_file_for_scenario__intercept_get_scenario_type);//Writing the address to be jumped
	*/
	WriteJmp(BUILD_CACHE_FILE_FOR_SCENARIO__GET_SCENARIO_TYPE, _build_cache_file_for_scenario__intercept_get_scenario_type);


	//single_player_shared sharing
	
	//.text:005883DE                 call    BUILD_CACHE_FILE_FOR_SCENARIO__TAG_SHARING_LOAD_SHARED ; STR: "tag sharing: loading tag names from shared.map", "tag sharing: 
	PatchCall(BUILD_CACHE_FILE_FOR_SCENARIO__TAG_SHARING_LOAD_SHARED, h_BUILD_CACHE_FILE_FOR_SCENARIO__TAG_SHARING_LOAD_SHARED);//modifying the call to go to my h_function rather orignal

	H2PCTool.WriteLog("Single Player tag_sharing enabled");
}

void H2ToolPatches::remove_bsp_version_check()
{
	// allow tool to work with BSPs compliled by newer versions of tool.
	// downgrades the error you would get to a non-fatal one.
	BYTE bsp_version_check_return[] = { 0xB0, 0x01 };
	WriteBytes(0x00545D0F, bsp_version_check_return, sizeof(bsp_version_check_return));
}

void H2ToolPatches::disable_secure_file_locking()
{
	// allow other processes to read files open with fopen_s
	WriteValue(0x0074DDD6 + 1, _SH_DENYWR);
}

LPWSTR __crtGetCommandLineW_hook()
{
	typedef LPWSTR (*__crtGetCommandLineW_t)();
	__crtGetCommandLineW_t __crtGetCommandLineW_impl = reinterpret_cast<__crtGetCommandLineW_t>(0x00764EB3);

	wchar_t *real_cmd = __crtGetCommandLineW_impl();
	std::wstring fake_cmd = std::regex_replace(real_cmd, std::wregex(L"( pause_after_run| shared_tag_removal)"), L"");
	wcscpy(real_cmd, fake_cmd.c_str());
	return real_cmd;
}

void H2ToolPatches::fix_command_line()
{
	PatchCall(0x00751F83, __crtGetCommandLineW_hook);
}

void tag_dump(int tag_index)
{
	std::string old_name = tags::get_name(tag_index);

	std::string new_name = "dump\\" + old_name;

	printf("dumping tag '%s' as '%s' ***\n", old_name.c_str(), new_name.c_str());

	tags::rename_tag(tag_index, new_name);
	tags::save_tag(tag_index);
	tags::rename_tag(tag_index, old_name);
}

template<typename T>
void SetScriptIdx(T *element, std::string placement_script, scnr_tag *scenario)
{
	element->scriptIndex = NONE; // defaults to zero instead of none, so this fixes that
	str_trim(placement_script);
	if (!placement_script.empty())
	{
		auto script_idx = FIND_TAG_BLOCK_STRING(&scenario->scripts, sizeof(hs_scripts_block), offsetof(hs_scripts_block, name), placement_script);
		if (script_idx == NONE)
			printf("[%s] Can't find script \"%s\"\n", typeid(T).name(), placement_script.c_str());
		else
			element->scriptIndex = script_idx;
	}
}

char __cdecl scenario_write_patch_file_hook(int TAG_INDEX, int a2)
{
	typedef char (__cdecl *scenario_write_patch_file)(int TAG_INDEX, int a2);
	auto scenario_write_patch_file_impl = reinterpret_cast<scenario_write_patch_file>(0x0056A110);

	scnr_tag *scenario = tags::get_tag<scnr_tag>('scnr', TAG_INDEX);

	// fix the compiler not setting up AI orders right and causing weird things to happen with scripts
	for (size_t i = 0; i < scenario->orders.size; i++)
	{
		orders_block *order = &scenario->orders.data[i];
		std::string target_script = order->entryScript;
		SetScriptIdx(order, target_script, scenario);
	}

	// squad placement scripts are also broken
	for (size_t i = 0; i < scenario->squads.size; i++)
	{
		auto *squad = &scenario->squads.data[i];
		std::string placement_script = squad->placementScript;
		SetScriptIdx(squad, placement_script, scenario);

		for (size_t j = 0; j < squad->startingLocations.size; j++)
		{
			auto *starting_location = &squad->startingLocations.data[j];
			std::string placement_script = starting_location->placementScript;
			SetScriptIdx(starting_location, placement_script, scenario);
		}
	}

	
	// dump tags for debuging if requested
	if (conf.getBoolean("dump_tags_packaging", false)) {
		tag_dump(TAG_INDEX);

		for (size_t i = 0; i < scenario->structureBSPs.size; i++)
			tag_dump(scenario->structureBSPs.data[i].structureBSP.tag_index);
	}

	return scenario_write_patch_file_impl(TAG_INDEX, a2);
}

enum lightmapping_distributed_type : int
{
	local,
	slave,
	master
};

static lightmapping_distributed_type global_lightmap_control_distributed_type;

// fix code for distributing light mapping over multiple computers
void reenable_lightmap_farm()
{
	global_lightmap_control_distributed_type =
		static_cast<lightmapping_distributed_type>(numerical::range_limit(conf.getNumber("global_lightmap_control_distributed_type", 0), 0, 2));
	WriteValue(0xA73D7C, conf.getNumber("slave_count__maybe", 3));

	constexpr DWORD lightmap_distributed_type_offsets[] = {
		0x4B314F, 0x4B362D, 0x4BB402, 0x4BD5CF,
		0x4BD6C1, 0x4BD877, 0x4BD9C1, 0x4E27AF,
		0x4C7206, 0x4C70D9, 0x4C6FD6, 0x4C6E50,
		0x4C6DAE, 0x4C6B77, 0x4C6A2F, 0x4C1F5F,
		0x4C1039, 0x4BFCA9, 0x4BF6FC
	};

	for (DWORD offset : lightmap_distributed_type_offsets)
		WritePointer(offset + 2, &global_lightmap_control_distributed_type);
	WritePointer(0x4C6F40 + 1, &global_lightmap_control_distributed_type);
	WritePointer(0x4C6E7F + 1, &global_lightmap_control_distributed_type);

	NopFill(0x4C0F21, 6);
}

void H2ToolPatches::Initialize()
{
	H2PCTool.WriteLog("Dll Successfully Injected to H2Tool");
	wcout << "H2Toolz version: " << version << std::endl
		 << "Built on " __DATE__ " at " __TIME__ << std::endl;

	Increase_structure_import_size_Check();
	Increase_structure_bsp_geometry_check();
	AddExtraCommands();
	unlock_other_scenario_types_compiling();
	render_model_import_unlock();
	remove_bsp_version_check();
	disable_secure_file_locking();
	fix_hs_converters();
	fix_command_line();
	//enable_campaign_tags_sharing(); // Still crashes might need tag changes.

	std::string cmd = GetCommandLineA();
	if (cmd.find("shared_tag_removal") != string::npos)
		apply_shared_tag_removal_scheme();

	// hooks the last step after all preprocessing and before packing
	PatchCall(0x00588A66, scenario_write_patch_file_hook);

	if (conf.getBoolean("simulate_game", false))
	{
		BYTE wdp_initialize_patch[] = { 0xB0, wdp_type::_sapien };
		WriteArray(0x53AB40, wdp_initialize_patch);
		WriteValue(0xA77DD0, GetModuleHandleA(NULL)); // g_hinstance
		WritePointer(0xA77DE0, DefWindowProcW);
		wcscpy_s(reinterpret_cast<wchar_t*>(0xA77DE4), 0x40, L"halo");
	}

	if (conf.getBoolean("disable_debug_tag_names", false))
	{
		NopFill(0x589031, 5); // remove call to build_cache_file_add_debug_tag_names
	}

	patch_cache_writter();

	// stops it from clearing sound references based on scenario type
	if (conf.getBoolean("disable_sound_references_postprocessing", false))
	{
		WritePointer(0x9FCBDC, nullptr);
	}
	reenable_lightmap_farm();
}


void H2ToolPatches::AddExtraCommands()
{
	H2PCTool.WriteLog("Adding Extra Commands to H2Tool");
	constexpr BYTE k_number_of_old_tool_commands = 0xC;
	constexpr BYTE k_number_of_old_tool_commands_copied = k_number_of_old_tool_commands - 1;
	constexpr BYTE k_number_of_tool_commands_new = (k_number_of_old_tool_commands_copied) + NUMBEROF(h2tool_extra_commands);

	// Tool's original tool commands
	static s_tool_command** tool_import_classes = CAST_PTR(s_tool_command**, 0x97B6EC);
	// The new tool commands list which is made up of tool's
	// and [yelo_extra_tool_commands]
	static s_tool_command* tool_commands[k_number_of_tool_commands_new];

	// copy official tool commands
	for (auto i = 0, j = 0; i < k_number_of_old_tool_commands; i++)
	{
		// check if we should skip this command (progress-quest)
		if (i == 10)
			continue;
		tool_commands[j++] = tool_import_classes[i];
	}
	
	// copy yelo tool commands
	memcpy_s(&tool_commands[k_number_of_old_tool_commands_copied], sizeof(h2tool_extra_commands),
		h2tool_extra_commands, sizeof(h2tool_extra_commands));

	// Now I know my ABCs
	qsort_s(tool_commands, NUMBEROF(tool_commands), sizeof(s_tool_command*), s_tool_command_compare, NULL);


	// update references to the tool command definitions
	static constexpr DWORD tool_commands_references[] = {
		0x410596,
		0x41060E,
		0x412D86,
	};

	for (int x = 0; x < NUMBEROF(tool_commands_references); x++)
		WriteValue(tool_commands_references[x], tool_commands);
	
	// update code which contain the tool command definitions count
	static constexpr DWORD tool_commands_count[] = {
		0x4105E5,
		0x412D99,
	};

	for (int x = 0; x < NUMBEROF(tool_commands_count); x++)
		WriteValue(tool_commands_count[x], k_number_of_tool_commands_new);
}
