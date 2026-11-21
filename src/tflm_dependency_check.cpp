
#include <Arduino.h>

// Header chính của thư viện ESP_TF
#include <ESP_TF.h>

// Header chứa MicroMutableOpResolver
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"

// Header chứa MicroInterpreter
#include "tensorflow/lite/micro/micro_interpreter.h"

// Hàm kiểm tra khả năng biên dịch TFLM
void tflm_dependency_compile_check()
{
    tflite::MicroMutableOpResolver<1> resolver;

    Serial.println("[TFLM] MicroMutableOpResolver: FOUND");

    (void)resolver;
}
