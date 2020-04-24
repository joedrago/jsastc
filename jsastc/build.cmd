emcc -O2 --bind -s WASM=0 -s EXPORT_NAME="jsastc" -s ALLOW_MEMORY_GROWTH=1 -o jsastc.js jsastc.cpp -DJSASTC_EMSCRIPTEN=1 -I..\Source ^
    ..\Source\astc_averages_and_directions.cpp ^
    ..\Source\astc_block_sizes2.cpp ^
    ..\Source\astc_color_quantize.cpp ^
    ..\Source\astc_color_unquantize.cpp ^
    ..\Source\astc_compress_symbolic.cpp ^
    ..\Source\astc_compute_variance.cpp ^
    ..\Source\astc_decompress_symbolic.cpp ^
    ..\Source\astc_encoding_choice_error.cpp ^
    ..\Source\astc_error_metrics.cpp ^
    ..\Source\astc_find_best_partitioning.cpp ^
    ..\Source\astc_ideal_endpoints_and_weights.cpp ^
    ..\Source\astc_image.cpp ^
    ..\Source\astc_image_load_store.cpp ^
    ..\Source\astc_integer_sequence.cpp ^
    ..\Source\astc_kmeans_partitioning.cpp ^
    ..\Source\astc_mathlib.cpp ^
    ..\Source\astc_mathlib_softfloat.cpp ^
    ..\Source\astc_partition_tables.cpp ^
    ..\Source\astc_percentile_tables.cpp ^
    ..\Source\astc_pick_best_endpoint_format.cpp ^
    ..\Source\astc_platform_dependents.cpp ^
    ..\Source\astc_platform_isa_detection.cpp ^
    ..\Source\astc_quantization.cpp ^
    ..\Source\astc_symbolic_physical.cpp ^
    ..\Source\astc_toplevel.cpp ^
    ..\Source\astc_weight_align.cpp ^
    ..\Source\astc_weight_quant_xfer_tables.cpp
