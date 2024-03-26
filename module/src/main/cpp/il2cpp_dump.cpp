//
// Created by Perfare on 2020/7/4.
//

#include "il2cpp_dump.h"
#include <dlfcn.h>
#include <cstdlib>
#include <cstring>
#include <cinttypes>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <unistd.h>
#include "xdl.h"
#include "log.h"
#include "il2cpp-tabledefs.h"
#include "il2cpp-class.h"

#define DO_API(r, n, p) r (*n) p

#include "il2cpp-api-functions.h"

#undef DO_API

static uint64_t il2cpp_base = 0;

void init_il2cpp_api(void *handle) {
#define DO_API(r, n, p) {                      \
    n = (r (*) p)xdl_sym(handle, #n, nullptr); \
    if(!n) {                                   \
        LOGW("api not found %s", #n);          \
    }                                          \
}

#include "il2cpp-api-functions.h"

#undef DO_API
}

std::string get_method_modifier(uint32_t flags) {
    std::stringstream outPut;
    auto access = flags & METHOD_ATTRIBUTE_MEMBER_ACCESS_MASK;
    switch (access) {
        case METHOD_ATTRIBUTE_PRIVATE:
            outPut << "private ";
            break;
        case METHOD_ATTRIBUTE_PUBLIC:
            outPut << "public ";
            break;
        case METHOD_ATTRIBUTE_FAMILY:
            outPut << "protected ";
            break;
        case METHOD_ATTRIBUTE_ASSEM:
        case METHOD_ATTRIBUTE_FAM_AND_ASSEM:
            outPut << "internal ";
            break;
        case METHOD_ATTRIBUTE_FAM_OR_ASSEM:
            outPut << "protected internal ";
            break;
    }
    if (flags & METHOD_ATTRIBUTE_STATIC) {
        outPut << "static ";
    }
    if (flags & METHOD_ATTRIBUTE_ABSTRACT) {
        outPut << "abstract ";
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_REUSE_SLOT) {
            outPut << "override ";
        }
    } else if (flags & METHOD_ATTRIBUTE_FINAL) {
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_REUSE_SLOT) {
            outPut << "sealed override ";
        }
    } else if (flags & METHOD_ATTRIBUTE_VIRTUAL) {
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_NEW_SLOT) {
            outPut << "virtual ";
        } else {
            outPut << "override ";
        }
    }
    if (flags & METHOD_ATTRIBUTE_PINVOKE_IMPL) {
        outPut << "extern ";
    }
    return outPut.str();
}

bool _il2cpp_type_is_byref(const Il2CppType *type) {
    auto byref = type->byref;
    if (il2cpp_type_is_byref) {
        byref = il2cpp_type_is_byref(type);
    }
    return byref;
}

std::string dump_method(Il2CppClass *klass) {
    std::stringstream outPut;
    outPut << "\n\t// Methods\n";
    void *iter = nullptr;
    while (auto method = il2cpp_class_get_methods(klass, &iter)) {
        if (method->methodPointer) {
            outPut << "\t// libil2cpp.so + 0x";
            outPut << std::hex << (uint64_t) method->methodPointer - il2cpp_base;
        } else {
            outPut << "\t// no methodPointer";
        }
        outPut << "\n\t";
        auto flags = *(uint32_t *)(method + 0x4c);
        outPut << get_method_modifier(flags);
        outPut << il2cpp_method_get_name(method)
               << "(";
        auto param_count = il2cpp_method_get_param_count(method);
        for (int i = 0; i < param_count; ++i) {
            auto param = il2cpp_method_get_param(method, i);
            auto attrs = param->attrs;
            if (_il2cpp_type_is_byref(param)) {
                if (attrs & PARAM_ATTRIBUTE_OUT && !(attrs & PARAM_ATTRIBUTE_IN)) {
                    outPut << "out ";
                } else if (attrs & PARAM_ATTRIBUTE_IN && !(attrs & PARAM_ATTRIBUTE_OUT)) {
                    outPut << "in ";
                } else {
                    outPut << "ref ";
                }
            } else {
                if (attrs & PARAM_ATTRIBUTE_IN) {
                    outPut << "[In] ";
                }
                if (attrs & PARAM_ATTRIBUTE_OUT) {
                    outPut << "[Out] ";
                }
            }
            auto parameter_class = il2cpp_class_from_type(param);
//
            const char* (*Method$$GetParamName)(const MethodInfo*, uint32_t);
            *(void **)(&Method$$GetParamName) = (void *)(il2cpp_base + 0x1DC286C);
//
            outPut << il2cpp_class_get_name(parameter_class) << " "
                   << Method$$GetParamName(method, i);
            outPut << ", ";
        }
        if (param_count > 0) {
            outPut.seekp(-2, outPut.cur);
        }
        outPut << ") { }\n";
        //TODO GenericInstMethod
    }
    return outPut.str();
}

std::string dump_field(Il2CppClass *klass) {
    std::stringstream outPut;
    outPut << "\n\t// Fields\n";
    void *iter = nullptr;
    while (auto field = il2cpp_class_get_fields(klass, &iter)) {
        //TODO attribute
        outPut << "\t";
        auto attrs = il2cpp_field_get_flags(field);
        auto access = attrs & FIELD_ATTRIBUTE_FIELD_ACCESS_MASK;
        switch (access) {
            case FIELD_ATTRIBUTE_PRIVATE:
                outPut << "private ";
                break;
            case FIELD_ATTRIBUTE_PUBLIC:
                outPut << "public ";
                break;
            case FIELD_ATTRIBUTE_FAMILY:
                outPut << "protected ";
                break;
            case FIELD_ATTRIBUTE_ASSEMBLY:
            case FIELD_ATTRIBUTE_FAM_AND_ASSEM:
                outPut << "internal ";
                break;
            case FIELD_ATTRIBUTE_FAM_OR_ASSEM:
                outPut << "protected internal ";
                break;
        }
        if (attrs & FIELD_ATTRIBUTE_LITERAL) {
            outPut << "const ";
        } else {
            if (attrs & FIELD_ATTRIBUTE_STATIC) {
                outPut << "static ";
            }
            if (attrs & FIELD_ATTRIBUTE_INIT_ONLY) {
                outPut << "readonly ";
            }
        }
        auto field_type = il2cpp_field_get_type(field);
        auto field_class = il2cpp_class_from_type(field_type);
        outPut << il2cpp_class_get_name(field_class) << " " << il2cpp_field_get_name(field);
        /*
         * il2cpp_field_static_get_value is stripped
         * if (attrs & FIELD_ATTRIBUTE_LITERAL && is_enum) {
            uint64_t val = 0;
            il2cpp_field_static_get_value(field, &val);
            outPut << " = " << std::dec << val;
        } */
        outPut << "; // 0x" << std::hex << il2cpp_field_get_offset(field) << "\n";
    }
    return outPut.str();
}

std::string dump_type(Il2CppClass *klass) {
    LOGE("klass %p", klass);
    std::stringstream outPut;
    if (klass) {
        outPut << "\n// Namespace: " << il2cpp_class_get_namespace(klass) << "\n";
        auto flags = il2cpp_class_get_flags(klass);
        if (flags & TYPE_ATTRIBUTE_SERIALIZABLE) {
            outPut << "[Serializable]\n";
        }
        //TODO attribute
        auto is_valuetype = il2cpp_class_is_valuetype(klass);
        auto is_enum = il2cpp_class_is_enum(klass);
        auto visibility = flags & TYPE_ATTRIBUTE_VISIBILITY_MASK;
        switch (visibility) {
            case TYPE_ATTRIBUTE_PUBLIC:
            case TYPE_ATTRIBUTE_NESTED_PUBLIC:
                outPut << "public ";
                break;
            case TYPE_ATTRIBUTE_NOT_PUBLIC:
            case TYPE_ATTRIBUTE_NESTED_FAM_AND_ASSEM:
            case TYPE_ATTRIBUTE_NESTED_ASSEMBLY:
                outPut << "internal ";
                break;
            case TYPE_ATTRIBUTE_NESTED_PRIVATE:
                outPut << "private ";
                break;
            case TYPE_ATTRIBUTE_NESTED_FAMILY:
                outPut << "protected ";
                break;
            case TYPE_ATTRIBUTE_NESTED_FAM_OR_ASSEM:
                outPut << "protected internal ";
                break;
        }
        if (flags & TYPE_ATTRIBUTE_ABSTRACT && flags & TYPE_ATTRIBUTE_SEALED) {
            outPut << "static ";
        } else if (!(flags & TYPE_ATTRIBUTE_INTERFACE) && flags & TYPE_ATTRIBUTE_ABSTRACT) {
            outPut << "abstract ";
        } else if (!is_valuetype && !is_enum && flags & TYPE_ATTRIBUTE_SEALED) {
            outPut << "sealed ";
        }
        if (flags & TYPE_ATTRIBUTE_INTERFACE) {
            outPut << "interface ";
        } else if (is_enum) {
            outPut << "enum ";
        } else if (is_valuetype) {
            outPut << "struct ";
        } else {
            outPut << "class ";
        }
        outPut << il2cpp_class_get_name(klass); //TODO genericContainerIndex
        std::vector<std::string> extends;
        auto parent = il2cpp_class_get_parent(klass);
        if (!is_valuetype && !is_enum && parent) {
            auto parent_type = il2cpp_class_get_type(parent);
            if (parent_type->type != IL2CPP_TYPE_OBJECT) {
                extends.emplace_back(il2cpp_class_get_name(parent));
            }
        }
        void *iter = nullptr;
        while (auto itf = il2cpp_class_get_interfaces(klass, &iter)) {
            extends.emplace_back(il2cpp_class_get_name(itf));
        }
        if (!extends.empty()) {
            outPut << " : " << extends[0];
            for (int i = 1; i < extends.size(); ++i) {
                outPut << ", " << extends[i];
            }
        }
        outPut << "\n{";
        outPut << dump_field(klass);
        outPut << dump_method(klass);
        //TODO EventInfo
        outPut << "}\n";
    }
    return outPut.str();
}

void il2cpp_api_init(void *handle) {
    LOGI("il2cpp_handle: %p", handle);
    init_il2cpp_api(handle);
    if (il2cpp_domain_get_assemblies) {
        Dl_info dlInfo;
        if (dladdr((void *) il2cpp_domain_get_assemblies, &dlInfo)) {
            il2cpp_base = reinterpret_cast<uint64_t>(dlInfo.dli_fbase);
        }
        LOGI("il2cpp_base: %" PRIx64"", il2cpp_base);
    } else {
        LOGE("Failed to initialize il2cpp api.");
        return;
    }
    while (!il2cpp_is_vm_thread(nullptr)) {
        LOGI("Waiting for il2cpp_init...");
        sleep(1);
    }
    auto domain = il2cpp_domain_get();
    il2cpp_thread_attach(domain);
}

void il2cpp_dump(const char *outDir) {
    LOGI("dumping...");

    std::vector<std::string> imagesToDump = {};

    imagesToDump.push_back("Assembly-CSharp");
    imagesToDump.push_back("UnityEngine.CoreModule");
    imagesToDump.push_back("UnityEngine.PhysicsModule");

    Il2CppClass* (*Image$$GetType)(const Il2CppImage* image, size_t index);
    Il2CppClass *(*Class$$FromName)(const Il2CppImage *image, const char *namespaze, const char *name);
    *(void **)(&Image$$GetType) = (void *)(il2cpp_base + 0x1DC13F0);
    *(void **)(&Class$$FromName) = (void *)(il2cpp_base + 0x1D9646C);

    std::vector<std::string> outPuts;
    auto outPut = dump_type(Class$$FromName(il2cpp_assembly_get_image(il2cpp_domain_assembly_open(il2cpp_domain_get(), "Assembly-CSharp")),
                                            "Axlebolt.Standoff.Player", "PlayerController"));
    outPuts.push_back(outPut);
   //for (int i = 0; i < imagesToDump.size(); i++) {
   //    auto assembly = il2cpp_domain_assembly_open(il2cpp_domain_get(), imagesToDump[i].c_str());
   //    if (!assembly) {
   //        LOGE("Assembly for image %s - not found, skipping", imagesToDump[i].c_str());
   //        continue;
   //    }
   //    auto image = il2cpp_assembly_get_image(assembly);
   //    if (!image) {
   //        LOGE("Image for assembly %s - not found, skipping", imagesToDump[i].c_str());
   //        continue;
   //    }

   //    size_t typesCount = il2cpp_image_get_class_count(image);
   //    LOGI("%d types", (int) typesCount);

   //    for (int j = 0; j < typesCount; ++j) {
   //        auto klass = Image$$GetType(image, j);
   //        if (klass) {
   //            auto outPut = dump_type(klass);
   //            outPuts.push_back(outPut);
   //        }
   //    }
   //}

    LOGI("write dump file");
    auto outPath = std::string(outDir).append("/files/procode_dump.cs");
    std::ofstream outStream(outPath);
    auto count = outPuts.size();
    for (int i = 0; i < count; ++i) {
        outStream << outPuts[i];
    }
    outStream.close();
    LOGI("dump done!");
}