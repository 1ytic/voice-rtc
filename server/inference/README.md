# wav2letter++

## Model implementation
This `inference` folder copied from https://github.com/flashlight/wav2letter/tree/main/recipes/streaming_convnets/inference


## Model weights
```bash
for f in acoustic_model.bin tds_streaming.arch decoder_options.json feature_extractor.bin language_model.bin lexicon.txt tokens.txt ; do wget http://dl.fbaipublicfiles.com/wav2letter/inference/examples/model/${f} ; done
```