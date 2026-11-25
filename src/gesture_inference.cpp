
#include "gesture_inference.h"
#include "gesture_model_data.h"

#include <cstring>

#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"

// --------------------------------------------------
// MODEL CONFIGURATION
// --------------------------------------------------

static constexpr size_t TENSOR_ARENA_SIZE = 64 * 1024;

alignas(16) static uint8_t tensor_arena[TENSOR_ARENA_SIZE];

static const tflite::Model *gesture_model = nullptr;

static tflite::MicroMutableOpResolver<9> resolver;

static tflite::MicroInterpreter *interpreter = nullptr;

static const char *gesture_labels[] = {
    "fist",
    "no_hand",
    "open_palm",
    "thumbs_up",
    "victory"};

// --------------------------------------------------
// INITIALIZE TFLM
// --------------------------------------------------

bool gesture_ai_init()
{
    if (interpreter != nullptr)
        return true;

    Serial.println();
    Serial.println("[TFLM] Initializing gesture model...");

    // Load model
    gesture_model = tflite::GetModel(gesture_model_data);

    if (gesture_model == nullptr)
    {
        Serial.println("[TFLM] Model is NULL");
        return false;
    }

    // Check schema version
    if (gesture_model->version() != TFLITE_SCHEMA_VERSION)
    {
        Serial.printf(
            "[TFLM] Schema mismatch: model=%d runtime=%d\n",
            gesture_model->version(),
            TFLITE_SCHEMA_VERSION);

        return false;
    }

    Serial.printf(
        "[TFLM] Model schema: %d\n",
        gesture_model->version());

    // Register only operators used by this model
    // Register all operators used by the model

    if (resolver.AddQuantize() != kTfLiteOk)
        return false;

    if (resolver.AddConv2D() != kTfLiteOk)
        return false;

    if (resolver.AddMaxPool2D() != kTfLiteOk)
        return false;

    if (resolver.AddShape() != kTfLiteOk)
        return false;

    if (resolver.AddStridedSlice() != kTfLiteOk)
        return false;

    if (resolver.AddPack() != kTfLiteOk)
        return false;

    if (resolver.AddReshape() != kTfLiteOk)
        return false;

    if (resolver.AddFullyConnected() != kTfLiteOk)
        return false;

    if (resolver.AddSoftmax() != kTfLiteOk)
        return false;

    Serial.println("[TFLM] All 9 model operators registered");

    // Static interpreter: create only once
    static tflite::MicroInterpreter static_interpreter(
        gesture_model,
        resolver,
        tensor_arena,
        sizeof(tensor_arena));

    if (static_interpreter.AllocateTensors() != kTfLiteOk)
    {
        Serial.println("[TFLM] AllocateTensors FAILED");
        return false;
    }

    interpreter = &static_interpreter;

    // Check input
    TfLiteTensor *input = interpreter->input(0);

    if (input == nullptr ||
        input->type != kTfLiteUInt8 ||
        input->dims->size != 4 ||
        input->dims->data[0] != 1 ||
        input->dims->data[1] != 64 ||
        input->dims->data[2] != 64 ||
        input->dims->data[3] != 1)
    {
        Serial.println("[TFLM] Invalid input tensor");
        interpreter = nullptr;
        return false;
    }

    // Check output
    TfLiteTensor *output = interpreter->output(0);

    if (output == nullptr ||
        output->type != kTfLiteUInt8 ||
        output->dims->size != 2 ||
        output->dims->data[0] != 1 ||
        output->dims->data[1] != 5)
    {
        Serial.println("[TFLM] Invalid output tensor");
        interpreter = nullptr;
        return false;
    }

    Serial.println("[TFLM] Input tensor OK");
    Serial.printf(
        "[TFLM] Input scale=%f zero_point=%d\n",
        input->params.scale,
        input->params.zero_point);

    Serial.println("[TFLM] Output tensor OK");
    Serial.printf(
        "[TFLM] Output scale=%f zero_point=%d\n",
        output->params.scale,
        output->params.zero_point);

    Serial.printf(
        "[TFLM] Tensor arena used: %u / %u bytes\n",
        (unsigned int)interpreter->arena_used_bytes(),
        (unsigned int)TENSOR_ARENA_SIZE);

    Serial.println("[TFLM] Initialization SUCCESS");

    return true;
}

// --------------------------------------------------
// RUN INFERENCE
// --------------------------------------------------

bool gesture_ai_predict(
    const uint8_t *pixels,
    size_t length,
    GesturePrediction &result)
{
    if (interpreter == nullptr || pixels == nullptr)
        return false;

    if (length != GESTURE_INPUT_BYTES)
        return false;

    TfLiteTensor *input = interpreter->input(0);
    // Copy grayscale image from webcam into TFLM input tensor
    memcpy(
        input->data.uint8,
        pixels,
        GESTURE_INPUT_BYTES);
    // Copy grayscale UINT8 image directly.
    // Do NOT divide by 255.
    // DEBUG: Check input image statistics
    uint8_t minPixel = 255;
    uint8_t maxPixel = 0;
    uint32_t sumPixel = 0;

    for (int i = 0; i < GESTURE_INPUT_BYTES; i++)
    {
        uint8_t value = input->data.uint8[i];

        if (value < minPixel)
            minPixel = value;

        if (value > maxPixel)
            maxPixel = value;

        sumPixel += value;
    }

    float meanPixel =
        (float)sumPixel / GESTURE_INPUT_BYTES;

    Serial.printf(
        "[TFLM] Input: min=%u max=%u mean=%.2f\n",
        minPixel,
        maxPixel,
        meanPixel);

    // DEBUG: Calculate input frame hash
    uint32_t hash = 2166136261u;

    for (int i = 0; i < GESTURE_INPUT_BYTES; i++)
    {
        hash ^= input->data.uint8[i];
        hash *= 16777619u;
    }

    Serial.printf("[TFLM] Frame hash: %08lX\n",
                  (unsigned long)hash);

    // DEBUG: Sample pixels from different regions
    Serial.printf(
        "[TFLM] Samples: %u %u %u %u %u %u %u %u %u\n",
        input->data.uint8[0],
        input->data.uint8[512],
        input->data.uint8[1024],
        input->data.uint8[1536],
        input->data.uint8[2048],
        input->data.uint8[2560],
        input->data.uint8[3072],
        input->data.uint8[3584],
        input->data.uint8[4095]);

    uint32_t start = millis();

    if (interpreter->Invoke() != kTfLiteOk)
    {
        Serial.println("[TFLM] Invoke FAILED");
        return false;
    }

    uint32_t elapsed = millis() - start;

    TfLiteTensor *output = interpreter->output(0);

    // DEBUG: Display all 5 class probabilities
    Serial.printf("[TFLM] All probabilities: ");

    for (int i = 0; i < 5; i++)
    {
        float probability =
            (output->data.uint8[i] - output->params.zero_point) * output->params.scale;

        Serial.printf("%s=%.3f ",
                      gesture_labels[i],
                      probability);
    }

    Serial.println();

    int best_index = 0;
    uint8_t best_value = output->data.uint8[0];

    for (int i = 1; i < 5; i++)
    {
        if (output->data.uint8[i] > best_value)
        {
            best_value = output->data.uint8[i];
            best_index = i;
        }
    }

    // Dequantize UINT8 output to probability
    float confidence =
        (best_value - output->params.zero_point) * output->params.scale;

    result.class_index = best_index;
    result.confidence = confidence;
    result.inference_ms = elapsed;
    result.label = gesture_labels[best_index];

    Serial.printf(
        "[TFLM] Prediction: %s | confidence: %.3f | time: %lu ms\n",
        result.label,
        result.confidence,
        (unsigned long)elapsed);

    return true;
}
