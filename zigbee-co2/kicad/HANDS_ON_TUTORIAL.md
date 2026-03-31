# Hands-On KiCad Tutorial: ESP32-C6 Zigbee CO2 Sensor

## Getting Started

### Open KiCad and Create Project
```
1. Launch KiCad from Applications folder
2. Click "File" → "New Project"
3. Name: "zigbee-co2-tutorial"
4. Location: Your chosen directory
5. Click "Create Project"
```

### Initial Project Setup
```
1. Double-click on "zigbee-co2-tutorial.kicad_sch" to open schematic editor
2. Click "File" → "Page Settings"
3. Set paper size to "A4"
4. Set orientation to "Landscape"
5. Click OK
```

## Lesson 1: Adding Components

### Step 1: Add USB-C Connector
```
1. Press 'A' key or click "Place" → "Symbol"
2. In the filter box, type: "USB_C_Receptacle_USB2.0"
3. Select from "Connector" library
4. Click "Place Symbol"
5. Position on left side of schematic
6. Press ESC to exit placement mode
```

### Step 2: Add Voltage Regulator
```
1. Press 'A' key
2. Filter: "AMS1117-3.3"
3. Select from "Regulator_Linear" library
4. Place to the right of USB-C connector
```

### Step 3: Add ESP32-C6
```
1. Press 'A' key
2. Filter: "ESP32-C6"
3. Look in Speed Studio library or create custom symbol
4. Place in center of schematic
5. Press ESC when done
```

### Step 4: Add Passive Components
```
Capacitors:
1. Press 'A' key
2. Filter: "C_Small"
3. Place 3 capacitors:
   - C1: Input capacitor (10uF)
   - C2: Output capacitor (10uF) 
   - C3: Decoupling capacitor (100nF)

Resistors:
1. Press 'A' key
2. Filter: "R_Small"
3. Place 3 resistors:
   - R1: LED resistor (330Ω)
   - R2: I2C pull-up (4.7K)
   - R3: I2C pull-up (4.7K)

LED:
1. Press 'A' key
2. Filter: "LED_Small"
3. Place one LED

Button:
1. Press 'A' key
2. Filter: "SW_Push"
3. Place one push button
```

## Lesson 2: Creating Power Nets

### Step 1: Add Power Ports
```
1. Press 'P' key or click "Place" → "Power Port"
2. Select "+5V" power port
3. Place near USB-C VBUS pin
4. Place "+3.3V" power port near regulator output
5. Place "GND" power ports at ground connections
```

### Step 2: Connect Power
```
1. Press 'W' key for wire tool
2. Connect USB-C VBUS to +5V port
3. Connect +5V port to AMS1117 VIN pin
4. Connect AMS1117 VOUT to +3.3V port
5. Connect all GND pins to GND ports
```

## Lesson 3: Connecting Components

### Step 1: Power Supply Connections
```
1. Connect USB-C VBUS to AMS1117 VIN pin
2. Connect USB-C GND to AMS1117 GND pin
3. Connect AMS1117 VOUT to +3.3V net
4. Place C1 between VIN and GND
5. Place C2 between VOUT and GND
```

### Step 2: ESP32-C6 Connections
```
Power:
1. Connect ESP32-C6 VDD to +3.3V
2. Connect ESP32-C6 GND to GND
3. Place C3 close to VDD pin (decoupling)

I2C:
1. Connect GPIO6 to SDA net
2. Connect GPIO7 to SCL net
3. Add net labels: Right-click → "Label" → "SDA", "SCL"
```

### Step 3: Sensor Connections
```
1. Create SCD40 symbol or use generic I2C sensor
2. Connect VDD to +3.3V
3. Connect GND to GND
4. Connect SDA to GPIO6
5. Connect SCL to GPIO7
6. Add pull-up resistors R2 and R3 on SDA/SCL lines
```

### Step 4: Status LED
```
1. Connect LED anode to R1 (330Ω)
2. Connect R1 other end to GPIO8
3. Connect LED cathode to GND
4. Add net label "LED_STATUS" on GPIO8
```

### Step 5: Reset Button
```
1. Connect one side of button to EN pin
2. Connect other side to GND
3. Add pull-up resistor (10K) from EN to +3.3V
4. Add net label "RESET" on EN pin
```

## Lesson 4: Adding Net Labels

### Step 1: Create Net Labels
```
1. Press 'L' key or click "Place" → "Label"
2. Type net name and press Enter
3. Click on wire to attach label
4. Add these labels:
   - "SDA" on I2C data line
   - "SCL" on I2C clock line
   - "LED_STATUS" on LED control line
   - "RESET" on reset line
   - "D+" on USB data plus
   - "D-" on USB data minus
```

### Step 2: Add Component Values
```
1. Double-click on each component
2. Set values:
   - C1: 10uF
   - C2: 10uF
   - C3: 100nF
   - R1: 330R
   - R2: 4.7K
   - R3: 4.7K
   - R4: 10K (reset pull-up)
3. Add reference designators if missing
```

## Lesson 5: Electrical Rules Check

### Step 1: Run ERC
```
1. Click "Inspect" → "Electrical Rules Check"
2. Click "Run ERC"
3. Review any errors or warnings
4. Common issues to fix:
   - Unconnected pins
   - Power pins not connected
   - Multiple outputs connected together
```

### Step 2: Fix Issues
```
1. Double-click on error in ERC report
2. Navigate to problematic area
3. Fix connection issues
4. Re-run ERC until clean
5. Save schematic (Ctrl+S)
```

## Lesson 6: Footprint Assignment

### Step 1: Open Footprint Assignment Tool
```
1. Click "Tools" → "Assign Footprints"
2. Review each component footprint
3. Click on component to change footprint
```

### Step 2: Assign Footprints
```
USB-C Connector:
1. Find: "USB_C_Receptacle_16Pin"
2. Library: Connector

AMS1117:
1. Find: "SOT-223-3_TabPin2"
2. Library: Package_SO

ESP32-C6:
1. Find: "ESP32-C6-MINI-1"
2. Library: ESP32-C6

Capacitors/Resistors:
1. Find: "R_0603_1608Metric" for resistors
2. Find: "C_0603_1608Metric" for capacitors
3. Library: Resistor_SMD, Capacitor_SMD

LED:
1. Find: "LED_0603_1608Metric"
2. Library: LED_SMD

Button:
1. Find: "Button_Switch_THT_6x6mm"
2. Library: Button_Switch_THT
```

### Step 3: Save Assignments
```
1. Click "Apply" to save changes
2. Click "OK" to close tool
3. Save schematic again
```

## Lesson 7: PCB Layout

### Step 1: Open PCB Editor
```
1. Close schematic editor
2. Double-click "zigbee-co2-tutorial.kicad_pcb"
3. Click "Tools" → "Update PCB from Schematic"
4. Click "Update Schematic Footprints"
5. Click "OK"
6. Click "Save" to confirm
```

### Step 2: Set Board Outline
```
1. Select "Edge.Cuts" layer from toolbar
2. Click "Add graphic line" or press 'Shift+L'
3. Draw rectangle:
   - Start at (50, 30)
   - End at (150, 90)
   - Creates 100mm x 60mm board
4. Close the shape
```

### Step 3: Place Components
```
Power Section:
1. Drag USB-C connector to left edge
2. Place AMS1117 near USB-C
3. Place C1 and C2 close to regulator pins

Microcontroller:
1. Place ESP32-C6 in center
2. Place C3 (decoupling) very close to power pins

Sensor Area:
1. Place SCD40 away from heat sources
2. Place R2 and R3 near sensor connections

User Interface:
1. Place LED near top edge (visible)
2. Place button in accessible location
```

### Step 4: Route Power First
```
1. Select "F.Cu" layer (top)
2. Press 'X' key for route tool
3. Route 5V and 3.3V traces:
   - Use 0.5mm width for power
   - Keep traces short and direct
4. Route ground connections
```

### Step 5: Add Ground Plane
```
1. Select "B.Cu" layer (bottom)
2. Click "Add filled zone" or press 'B'
3. Draw rectangle covering entire board
4. Double-click to set properties
5. Set net to "GND"
6. Click "OK"
7. Press 'B' key to fill all zones
```

### Step 6: Route Signal Traces
```
1. Stay on "F.Cu" layer
2. Route I2C signals (SDA, SCL):
   - Keep traces short
   - Route together if possible
3. Route USB signals (D+, D-):
   - Keep parallel and close together
4. Route remaining GPIO connections
```

### Step 7: Design Rule Check
```
1. Click "Inspect" → "Design Rules Checker"
2. Click "Run DRC"
3. Fix any violations:
   - Trace too thin
   - Clearance too small
   - Unconnected items
4. Re-run until clean
```

## Lesson 8: Final Checks

### Step 1: 3D Visualization
```
1. Click "View" → "3D Viewer"
2. Rotate to inspect component placement
3. Check for any obvious issues
4. Close 3D viewer
```

### Step 2: Net Inspector
```
1. Click "Tools" → "Net Inspector"
2. Verify all critical nets are connected:
   - +5V
   - +3.3V
   - GND
   - SDA, SCL
   - LED_STATUS
   - RESET
```

### Step 3: Save Everything
```
1. Save PCB layout (Ctrl+S)
2. Close all KiCad windows
3. Check project folder for all files
```

## Lesson 9: Generate Manufacturing Files

### Step 1: Plot Gerbers
```
1. Open PCB layout
2. Click "File" → "Plot"
3. Select these layers:
   - F.Cu (Top copper)
   - B.Cu (Bottom copper)
   - F.SilkS (Top silkscreen)
   - B.SilkS (Bottom silkscreen)
   - F.Mask (Top solder mask)
   - B.Mask (Bottom solder mask)
   - Edge.Cuts (Board outline)
4. Set output directory
5. Click "Plot"
```

### Step 2: Generate Drill File
```
1. In same Plot dialog
2. Click "Generate Drill Files"
3. Select "Excellon format"
4. Set units to "mm"
5. Click "Generate"
```

### Step 3: Generate Position File
```
1. Click "File" → "Fabrication Outputs" → "Footprint Position (.pos)"
2. Set format to "CSV"
3. Set units to "mm"
4. Click "Save"
```

### Step 4: Generate BOM
```
1. Open schematic editor
2. Click "Tools" → "Generate Bill of Materials"
3. Select fields: Reference, Value, Footprint, Datasheet
4. Click "Generate"
5. Save as CSV
```

## Lesson 10: Ready for Manufacturing

### File Organization
```
Create folder structure:
├── gerber_files/
│   ├── *.gbr (all Gerber files)
│   └── *.drl (drill file)
├── assembly_files/
│   ├── *.pos (position file)
│   └── BOM.csv
└── documentation/
    ├── schematic.pdf
    └── layout.pdf
```

### Upload to Manufacturer
```
1. Go to JLCPCB.com
2. Create account
3. Start new order
4. Upload Gerber zip file
5. Configure parameters:
   - Layers: 2
   - Thickness: 1.6mm
   - Material: FR4
   - Surface finish: HASL Lead Free
6. Review and order
```

## Common Issues and Solutions

### Schematic Issues
- **Unconnected pins**: Use wire tool to connect
- **Missing power ports**: Add power symbols
- **Duplicate references**: Renumber components

### PCB Layout Issues
- **DRC violations**: Increase trace width or clearance
- **Unrouted connections**: Complete routing
- **Component overlap**: Move components apart

### Manufacturing Issues
- **Missing files**: Generate all required outputs
- **File format errors**: Use correct settings
- **Design rule violations**: Fix before ordering

## Next Steps

After completing this tutorial:
1. Practice with different component layouts
2. Try more complex designs
3. Learn about high-speed design considerations
4. Explore advanced KiCad features

---

This hands-on tutorial will guide you through every step of creating your ESP32-C6 Zigbee CO2 sensor PCB. Take your time and practice each step until you're comfortable with the process.
