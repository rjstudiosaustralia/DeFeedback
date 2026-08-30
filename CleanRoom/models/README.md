# Models

No trained weights are committed at this milestone.

A future model package must include:

- architecture and input/output tensor contract;
- sample rate, hop size, receptive field, FIR length, and measured algorithmic latency;
- recurrent-state reset semantics;
- training-data manifest and licences;
- conversion script and source-model checksum;
- BNNS Graph and Core ML compatibility report;
- numerical parity tests against the training framework;
- callback benchmark results for every supported Mac class;
- a verified identity/failure path.

Do not add Alpha Labs binaries, output-derived teacher data, model resources, or activation/licence files to this directory.
