/dts-v1/;

/ {
    description = "Chrome OS kernel image with one or more FDT blobs";
    images {
        kernel-1{
            description = "kernel";
            data = /incbin/("arch/arm64/boot/Image");
            type = "kernel_noload";
            arch = "arm64";
            os = "linux";
            compression = "none";
            load = <0>;
            entry = <0>;
        };
        fdt-1{
            description = "mt8173-elm.dtb";
            data = /incbin/("arch/arm64/boot/dts/mediatek/mt8173-elm.dtb");
            type = "flat_dt";
            arch = "arm64";
            compression = "none";
            hash-1{
                algo = "sha1";
            };
        };
        fdt-2{
            description = "mt8173-elm-hana.dtb";
            data = /incbin/("arch/arm64/boot/dts/mediatek/mt8173-elm-hana.dtb");
            type = "flat_dt";
            arch = "arm64";
            compression = "none";
            hash-1{
                algo = "sha1";
            };
        };
        fdt-3{
            description = "mt8173-elm-hana-rev7.dtb";
            data = /incbin/("arch/arm64/boot/dts/mediatek/mt8173-elm-hana-rev7.dtb");
            type = "flat_dt";
            arch = "arm64";
            compression = "none";
            hash-1{
                algo = "sha1";
            };
        };
    };
    configurations {
        default = "conf-1";
        conf-1{
            kernel = "kernel-1";
            fdt = "fdt-1";
        };
        conf-2{
            kernel = "kernel-1";
            fdt = "fdt-2";
        };
        conf-3{
            kernel = "kernel-1";
            fdt = "fdt-3";
        };
    };
};
