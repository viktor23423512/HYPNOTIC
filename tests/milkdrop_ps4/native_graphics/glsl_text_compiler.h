#pragma once
#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/resource_limits_c.h>

// Actual runtime GLSL -> SPIR-V -> NIR/ACO -> native shader compilation.
// This test uses explicit Vulkan-compatible GLSL, not projectM's GL dialect yet.
static PsbcShaderOutput compileText(PsbcTarget target,PsbcStage stage,const char *text,
                                   const PsbcCompileOptions *resourceOptions=nullptr)
{
    notify(stage==PSBC_STAGE_VERTEX?"COMPILING VERTEX SHADER TEXT":"COMPILING FRAGMENT SHADER TEXT");
    const uint64_t started=sceKernelGetProcessTime();
    glslang_input_t input={};
    input.language=GLSLANG_SOURCE_GLSL;
    input.stage=stage==PSBC_STAGE_VERTEX?GLSLANG_STAGE_VERTEX:GLSLANG_STAGE_FRAGMENT;
    input.client=GLSLANG_CLIENT_VULKAN;
    input.client_version=GLSLANG_TARGET_VULKAN_1_0;
    input.target_language=GLSLANG_TARGET_SPV;
    input.target_language_version=GLSLANG_TARGET_SPV_1_0;
    input.code=text;input.default_version=450;input.default_profile=GLSLANG_CORE_PROFILE;
    input.messages=(glslang_messages_t)(GLSLANG_MSG_SPV_RULES_BIT|GLSLANG_MSG_VULKAN_RULES_BIT);
    input.resource=glslang_default_resource();
    glslang_shader_t *shader=glslang_shader_create(&input);
    if(!shader)fail("GLSL SHADER ALLOCATION",-1);
    if(!glslang_shader_preprocess(shader,&input) || !glslang_shader_parse(shader,&input))
    {
        note("GLSL ERROR: %s",glslang_shader_get_info_log(shader));
        printf("MILKDROP GLSL DEBUG: %s\n",glslang_shader_get_info_debug_log(shader));
        glslang_shader_delete(shader);fail("GLSL PARSE",-1);
    }
    glslang_program_t *program=glslang_program_create();
    if(!program)fail("GLSL PROGRAM ALLOCATION",-1);
    glslang_program_add_shader(program,shader);
    if(!glslang_program_link(program,input.messages))
    {
        note("GLSL LINK ERROR: %s",glslang_program_get_info_log(program));
        glslang_program_delete(program);glslang_shader_delete(shader);fail("GLSL LINK",-1);
    }
    glslang_spv_options_t options={};options.disable_optimizer=true;
    glslang_program_SPIRV_generate_with_options(program,input.stage,&options);
    const size_t count=glslang_program_SPIRV_get_size(program);
    const uint32_t *words=glslang_program_SPIRV_get_ptr(program);
    if(!words || count<5 || count>262144 || words[0]!=0x07230203)
    {
        note("GLSL SPIRV ERROR: %s",glslang_program_SPIRV_get_messages(program));
        glslang_program_delete(program);glslang_shader_delete(shader);fail("GLSL SPIRV OUTPUT",-1);
    }
    printf("MILKDROP GLSL %s GENERATED %u SPIRV WORDS IN %u MS\n",stage==PSBC_STAGE_VERTEX?"VS":"PS",(unsigned)count,(unsigned)((sceKernelGetProcessTime()-started)/1000));
    PsbcShaderOutput output={};
    if(resourceOptions) {
        const PsbcResult rc=psbc_compile_shader(words,count*4,resourceOptions,&output);
        if(rc!=PSBC_RESULT_OK)fail("RESOURCE SHADER COMPILATION",rc);
        printf("MILKDROP RESOURCE SHADER COMPILED %u BYTES\n",(unsigned)output.size);
    } else output=compile(target,stage,words,count*4);
    glslang_program_delete(program);glslang_shader_delete(shader);
    return output;
}
