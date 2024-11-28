# Mixed Event Routine

This repository provides a tool to perform an **Event Mixing** procedure offline.  
Event Mixing pairs tracks from different events to provide background information for analysis.

---

## Requirements

To use this tool, ensure the following dependencies are installed:

- **ROOT**: A framework for data processing, widely used in high-energy physics.  
- **yaml-cpp**: A YAML parser and emitter in C++.

---

## How It Works

1. **Input Dataset**:  
   - The code processes input datasets in the form of a **TTree**.  
   - Multiple TTrees can be merged along the horizontal axis or sequentially (one after another).

2. **Configuration**:  
   - The event mixing procedure is managed via a configuration file in YAML format.  
   - Update the configuration file to specify input paths and output file locations.

3. **Running the Routine**:  
   Execute the Event Mixing Routine from the ROOT terminal using the following commands:

   ```cpp
   .x load.cpp
   MixedEventInterface("/path/to/your-config-file.yml")
   ```
