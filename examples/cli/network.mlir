module {
  func.func @main(%arg0: tensor<2xf32>) -> tensor<2xf32> {
    %0 = stablehlo.negate %arg0 : tensor<2xf32>
    %1 = stablehlo.negate %0 : tensor<2xf32>
    %2 = stablehlo.abs %1 : tensor<2xf32>
    return %2 : tensor<2xf32>
  }
}
