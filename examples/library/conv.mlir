module {
  func.func @main(%x: tensor<1x1x3x3xf32>, %w: tensor<1x1x1x1xf32>) -> tensor<1x1x3x3xf32> {
    %0 = "stablehlo.convolution"(%x, %w) {window_strides = array<i64: 1, 1>, padding = dense<0> : tensor<2x2xi64>,
      lhs_dilation = array<i64: 1, 1>, rhs_dilation = array<i64: 1, 1>,
      window_reversal = array<i1: false, false>,
      dimension_numbers = #stablehlo.conv<[b, f, 0, 1]x[o, i, 0, 1]->[b, f, 0, 1]>,
      batch_group_count = 1 : i64, feature_group_count = 1 : i64,
      precision_config = [#stablehlo<precision DEFAULT>, #stablehlo<precision DEFAULT>]} : (tensor<1x1x3x3xf32>, tensor<1x1x1x1xf32>) -> tensor<1x1x3x3xf32>
    %1 = "stablehlo.convolution"(%0, %w) {window_strides = array<i64: 1, 1>, padding = dense<0> : tensor<2x2xi64>,
      lhs_dilation = array<i64: 1, 1>, rhs_dilation = array<i64: 1, 1>,
      window_reversal = array<i1: false, false>,
      dimension_numbers = #stablehlo.conv<[b, f, 0, 1]x[o, i, 0, 1]->[b, f, 0, 1]>,
      batch_group_count = 1 : i64, feature_group_count = 1 : i64,
      precision_config = [#stablehlo<precision DEFAULT>, #stablehlo<precision DEFAULT>]} : (tensor<1x1x3x3xf32>, tensor<1x1x1x1xf32>) -> tensor<1x1x3x3xf32>
    return %1 : tensor<1x1x3x3xf32>
  }
}
