load("//build/kernel/kleaf:kernel.bzl", "ddk_module")
load("//build/bazel_common_rules/dist:dist.bzl", "copy_to_dist_dir")

def hsi2s_get_srcs():
    srcs = [
        "driver/hsi2s_drv.c",
        "driver/hsi2s_param.c",
        "driver/lemans.c",
        "driver/nord.c",
    ]

    return srcs

def define_target_variant_module(target, variant):
    """
    Generates the ddk_module for each of our kernel modules
    Args:
        target: either `pineapple` or `kalama`
        variant: either `gki` or `consolidate`
        config_options: decides which kernel modules to build
    """
    print("target= ", target)
    print("variant= ", variant)

    tv = "{}_{}".format(target, variant)
    rule_name = "{}_hsi2s".format(tv)
    kernel_build = select({
        "//build/kernel/kleaf:socrepo_true": "//soc-repo:{}_base_kernel".format(tv),
        "//build/kernel/kleaf:socrepo_false": "//msm-kernel:{}".format(tv),
    })

    print("kernel_build=", kernel_build)

    ddk_module(
        name = rule_name,
        out = "hsi2s.ko",
        srcs = hsi2s_get_srcs(),
        hdrs = ["driver/hsi2s_drv.h",
                "driver/hsi2s_common.h",
                "driver/hsi2s_adsp_clk_ctrl.h",
                "driver/hsi2s_param.h",
                "driver/log.h",
                "driver/target_ops.h"
        ],

        includes = ["include"],
        deps = ["//common:all_headers",
                "//soc-repo:all_headers",
                "//soc-repo:{}/drivers/soc/qcom/hab/msm_hab".format(tv),
                "//soc-repo:{}/drivers/pinctrl/qcom/pinctrl-msm".format(tv),
            ],
        kernel_build = kernel_build,
        visibility = ["//visibility:public"]
    )

    copy_to_dist_dir(
        name = "{}_dist".format(rule_name),
        data = [rule_name],
        dist_dir = "out/target/product/{}/dlkm/lib/modules/".format(target),
        flat = True,
        wipe_dist_dir = False,
        allow_duplicate_filenames = False,
        mode_overrides = {"**/*": "644"},
        log = "info",
    )

def define_target_module(target):
    print("target=", target)
    define_target_variant_module(target, "perf")
    define_target_variant_module(target, "consolidate")
    define_target_variant_module(target, "gki")
