# Step-by-Step PCB Design Guide: ESP32-C6 Zigbee CO2 Sensor

## Overview
This guide will teach you how to design a complete PCB for your zigbee-co2 project using KiCad and the ESP32-C6 from Speed Studio.

## Step 1: Project Setup in KiCad

### 1.1 Create New Project
1. **Open KiCad** from Applications folder
2. **File → New Project**
3. **Name your project**: `zigbee-co2`
4. **Save location**: Choose your working directory
5. **Create project files** (KiCad will create .kicad_sch, .kicad_pcb, .kicad_pro, .kicad_sch-bak)

### 1.2 Set Project Properties
1. **File → Project Properties**
2. **Title**: "Zigbee CO2 Sensor"
3. **Company**: Your name/organization
4. **Revision**: "1.0"
5. **Date**: Current date

## Step 2: Component Library Setup

### 2.1 Add ESP32-C6 from Speed Studio
1. **Open Symbol Editor** (from KiCad main window)
2. **Preferences → Symbol Libraries**
3. **Add Library** → Browse to Speed Studio library
4. **If not available**: Download from Speed Studio website

### 2.2 Add Common Component Libraries
Ensure these libraries are enabled:
- **Device** (for resistors, capacitors, LEDs)
- **Connector** (for USB-C)
- **Regulator_Linear** (for AMS1117)
- **Switch** (for push buttons)

## Step 3: Schematic Design

### 3.1 Create Schematic
1. **Open Schematic Editor** (Eeschema)
2. **Page Settings**: Set to A4, landscape
3. **Grid**: Set to 50 mil for easier alignment

### 3.2 Add Components

#### Power Supply Section
1. **USB-C Connector**:
   - Place `USB_C_Receptacle_USB2.0` from Connector library
   - Position on left side of schematic
   - Connect VBUS, D+, D-, GND, CC pins

2. **Voltage Regulator**:
   - Place `AMS1117-3.3` from Regulator_Linear library
   - Connect: VIN to VBUS, GND to ground, VOUT to 3.3V
   - Add input/output capacitors (10uF and 100nF)

#### Microcontroller Section
1. **ESP32-C6**:
   - Find ESP32-C6 symbol in Speed Studio library
   - Place in center of schematic
   - Connect power pins to 3.3V and GND
   - Add decoupling capacitors (100nF near power pins)

#### Sensor Section
1. **SCD40 Sensor**:
   - Place SCD40 symbol (create if needed)
   - Connect VDD to 3.3V, GND to ground
   - Connect I2C: SDA to GPIO6, SCL to GPIO7
   - Add 4.7K pull-up resistors on SDA/SCL

#### Status Indicators
1. **Status LED**:
   - Place LED symbol from Device library
   - Add 330Ω current-limiting resistor
   - Connect to GPIO pin (e.g., GPIO8)

2. **Reset Button**:
   - Place push button from Switch library
   - Connect to EN pin with pull-up resistor

### 3.3 Connect Power Nets
1. **Create Power Labels**:
   - `+5V` for USB input
   - `+3V3` for regulated voltage
   - `GND` for ground
2. **Use Power Port symbols** for clean connections

### 3.4 Add Net Labels
1. **Label key signals**:
   - `SDA`, `SCL` for I2C
   - `RESET` for reset line
   - `LED_STATUS` for LED control
   - `D+`, `D-` for USB data

### 3.5 Electrical Rules Check
1. **Run ERC**: Inspect → Electrical Rules Check
2. **Fix all errors** before proceeding
3. **Save schematic**

## Step 4: Component Footprint Assignment

### 4.1 Assign Footprints
1. **Tools → Assign Footprints**
2. **Select components one by one**:
   - ESP32-C6: ESP32-C6-MINI-1 footprint
   - USB-C: USB_C_Receptacle_16Pin
   - AMS1117: SOT-223-3_TabPin2
   - Resistors/Capacitors: 0603 package
   - LED: LED_0603_1608Metric
   - Button: Button_Switch_THT_6x6mm

### 4.2 Verify Footprints
1. **Check each footprint** matches physical component
2. **Ensure pin numbering** matches symbols
3. **Save footprint assignments**

## Step 5: PCB Layout Design

### 5.1 Import Schematic to PCB
1. **Open PCB Editor** (Pcbnew)
2. **Tools → Update PCB from Schematic**
3. **Confirm import** of all components and connections

### 5.2 Board Outline
1. **Set board size**: 100mm x 60mm
2. **Draw board outline** on Edge.Cuts layer
3. **Add mounting holes** (3.2mm) in corners

### 5.3 Component Placement

#### Power Supply Layout
1. **USB-C connector**: Place on left edge
2. **AMS1117 regulator**: Near USB-C input
3. **Input/output capacitors**: Close to regulator pins

#### Microcontroller Layout
1. **ESP32-C6**: Center of board
2. **Decoupling capacitors**: As close as possible to power pins
3. **Crystal/oscillator**: Near XTAL pins (if required)

#### Sensor Layout
1. **SCD40**: Away from heat sources
2. **I2C pull-ups**: Near sensor connector
3. **Keep sensor accessible** for calibration

#### User Interface
1. **LED**: Visible position (top edge)
2. **Reset button**: Accessible location
3. **Programming header**: Edge of board

### 5.4 Routing Strategy

#### Power Routing
1. **Route power first**: 5V, 3.3V, GND
2. **Use wider traces**: 0.5-1.0mm for power
3. **Create ground plane**: Fill bottom layer with copper

#### Signal Routing
1. **I2C signals**: Route together, keep short
2. **USB signals**: Keep differential pair close
3. **GPIO signals**: Route as needed

#### Design Rules
1. **Minimum trace width**: 0.2mm
2. **Minimum via size**: 0.4mm drill / 0.8mm diameter
3. **Minimum clearance**: 0.2mm

### 5.5 Ground Plane
1. **Select bottom layer (B.Cu)**
2. **Draw polygon fill** covering entire board
3. **Connect to GND net**
4. **Run Fill** (B key)

### 5.6 Design Rule Check
1. **Run DRC**: Inspect → Design Rules Checker
2. **Fix all violations**
3. **Verify clearances and trace widths**

## Step 6: Final Checks

### 6.1 Visual Inspection
1. **Check component orientations**
2. **Verify pin connections**
3. **Ensure proper clearances**

### 6.2 3D Preview
1. **View → 3D Viewer**
2. **Rotate and inspect** component placement
3. **Check for interference**

### 6.3 Netlist Verification
1. **Tools → Net Inspector**
2. **Verify all connections** are complete
3. **Check for unconnected pins**

## Step 7: Manufacturing Outputs

### 7.1 Generate Gerber Files
1. **File → Plot**
2. **Select layers**:
   - F.Cu (Top copper)
   - B.Cu (Bottom copper)
   - F.SilkS (Top silkscreen)
   - B.SilkS (Bottom silkscreen)
   - F.Mask (Top solder mask)
   - B.Mask (Bottom solder mask)
   - Edge.Cuts (Board outline)
3. **Set output format**: Gerber
4. **Set precision**: 6 digits
5. **Generate files**

### 7.2 Generate Drill Files
1. **File → Plot → Generate Drill Files**
2. **Select Excellon format**
3. **Set units to mm**
4. **Generate drill file**

### 7.3 Generate Pick and Place
1. **File → Fabrication Outputs → Footprint Position (.pos)**
2. **Select CSV format**
3. **Set units to mm**
4. **Generate position file**

### 7.4 Generate BOM
1. **Tools → Generate Bill of Materials**
2. **Select required fields**
3. **Export as CSV**

## Step 8: Ordering from Manufacturers

### 8.1 Prepare Files
1. **Zip all Gerber files**
2. **Include drill file**
3. **Add BOM and position files** (for assembly)

### 8.2 Upload to Manufacturer
1. **JLCPCB.com** (recommended)
2. **Create account and start order**
3. **Upload zip file**
4. **Configure parameters**:
   - Layers: 2
   - Thickness: 1.6mm
   - Material: FR4
   - Surface finish: HASL Lead Free
   - Solder mask: Green
   - Silkscreen: White

### 8.3 Review and Order
1. **Review design rule check** from manufacturer
2. **Approve design**
3. **Select shipping method**
4. **Complete payment**

## Tips and Best Practices

### Design Tips
1. **Keep high-frequency traces short**
2. **Use ground planes for EMI reduction**
3. **Place decoupling capacitors close to ICs**
4. **Consider thermal management**

### Common Mistakes to Avoid
1. **Forgot pull-up resistors on I2C**
2. **Insufficient power trace width**
3. **Poor ground plane design**
4. **Incorrect footprint assignment**

### Troubleshooting
1. **DRC errors**: Check minimum clearances
2. **Unconnected nets**: Verify schematic connections
3. **Footprint issues**: Check pin numbering

## Next Steps

After receiving your PCB:
1. **Inspect for quality**
2. **Test power supply**
3. **Flash ESP32-C6 firmware**
4. **Test sensor communication**
5. **Calibrate CO2 sensor**

---

This guide provides a complete foundation for designing your zigbee-co2 PCB. Take your time with each step and verify your work before proceeding to the next.
