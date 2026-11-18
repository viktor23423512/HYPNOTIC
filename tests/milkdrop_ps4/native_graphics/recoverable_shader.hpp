#pragma once
static bool tryCompileFragment(const char *text,const PsbcCompileOptions &native,PsbcShaderOutput &output,std::string &error){
    const auto stage=PSBC_STAGE_FRAGMENT;
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
    if(!shader){error="GLSL shader allocation failed";return false;}
    if(!glslang_shader_preprocess(shader,&input) || !glslang_shader_parse(shader,&input))
    {
        error=std::string("GLSL: ")+glslang_shader_get_info_log(shader);
        glslang_shader_delete(shader);return false;
    }
    glslang_program_t *program=glslang_program_create();
    if(!program){glslang_shader_delete(shader);error="GLSL program allocation failed";return false;}
    glslang_program_add_shader(program,shader);
    if(!glslang_program_link(program,input.messages))
    {
        error=std::string("GLSL link: ")+glslang_program_get_info_log(program);
        glslang_program_delete(program);glslang_shader_delete(shader);return false;
    }
    glslang_spv_options_t options={};options.disable_optimizer=true;
    glslang_program_SPIRV_generate_with_options(program,input.stage,&options);
    const size_t count=glslang_program_SPIRV_get_size(program);
    const uint32_t *words=glslang_program_SPIRV_get_ptr(program);
    if(!words || count<5 || count>65536 || words[0]!=0x07230203)
    {
        error="Invalid or excessive SPIR-V output";
        glslang_program_delete(program);glslang_shader_delete(shader);return false;
    }

    const auto rc=psbc_compile_shader(words,count*4,&native,&output);
    glslang_program_delete(program);glslang_shader_delete(shader);
    if(rc!=PSBC_RESULT_OK){psbc_free_output(&output);error="Native shader compiler returned "+std::to_string((int)rc);return false;}
    return true;
}
static GnmPsShader *tryPreparePixel(const PsbcShaderOutput &output,std::string &error){
    const size_t offset=0x24+sizeof(GnmShaderFileHeader);
    if(!output.data || output.size<offset+sizeof(GnmShaderCommonData)+8){error="Shader header is truncated";return nullptr;}
    const auto bytes=(const unsigned char *)output.data;uint32_t magic=0;memcpy(&magic,bytes+0x24,4);
    if(magic!=GNM_SHADER_FILE_HEADER_ID){error="Invalid native shader container";return nullptr;}
    const auto common=(const GnmShaderCommonData *)(bytes+offset);const auto addresses=(const uint32_t *)(common+1);
    const size_t codeOffset=offset+addresses[0],codeSize=sceGnmShaderCommonCodeSize(common);
    const size_t headerSize=sceGnmPsShaderCalcSize((const GnmPsShader *)common);
    if((addresses[0]&3) || addresses[1]!=0xffffffff || headerSize>output.size-offset || codeOffset>output.size || codeSize>output.size-codeOffset || codeSize>1024*1024){error="Native shader bounds invalid";return nullptr;}
    auto header=(GnmPsShader *)malloc(headerSize);if(!header){error="Shader header allocation failed";return nullptr;}
    void *code=gpuAlloc(codeSize,GNM_ALIGNMENT_SHADER_BYTES);if(!code){free(header);error="GPU shader pool exhausted";return nullptr;}
    memcpy(header,common,headerSize);memcpy(code,bytes+codeOffset,codeSize);sceGnmPsRegsSetAddress(&header->registers,code);header->registers.spibaryccntl=0;
    return header;
}
