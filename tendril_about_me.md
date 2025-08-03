# Tendril Hardware Project Plan - Complete 3D Modeling & Manufacturing Specifications

## 🎯 Project Overview

**Project Name**: RL-Powered Tendril Robotic Arm  
**Objective**: Build a 3-joint articulated robotic arm that learns precise pointing through reinforcement learning  
**Target Performance**: <5° pointing accuracy, 2+ second stability, 100mm workspace  
**Applications**: Precision laser pointer, camera gimbal, automated pointing device, RL research platform

**CRITICAL**: All dimensions in this document are extracted from the actual trained RL simulation code to ensure perfect sim-to-real transfer. **DO NOT modify these dimensions** without retraining the RL model.

## 📐 **EXACT DIMENSIONS FROM SIMULATION CODE**

All dimensions below are extracted directly from the trained RL environment code to ensure perfect sim-to-real matching.

### **Base Platform (Base.stl) - DETAILED SPECIFICATIONS**

#### **Primary Dimensions (CODE-EXACT)**
```c
#define BASE_WIDTH  60.0f    // 60mm × 60mm × 20mm
#define BASE_HEIGHT 60.0f    
#define BASE_DEPTH  20.0f    
```

#### **Detailed Design Requirements**

**Overall Envelope**: 60mm × 60mm × 20mm rectangular platform

**Servo 1 (Base Yaw) Mounting Pocket**:
- **Location**: Geometric center of base platform
- **Servo Body Dimensions**: 40.7mm × 19.7mm × 42.9mm (MG996R standard)
- **Mounting Pocket**: 41.0mm × 20.0mm × 15.0mm deep (0.15mm clearance each side)
- **Servo Tab Mounting**: 2× mounting ears, 10mm spacing, 2.5mm diameter holes
- **Shaft Access**: 6mm diameter center hole, 25mm deep minimum
- **Horn Clearance**: 30mm diameter × 5mm deep recess above servo

**Corner Mounting Points** (4× locations):
- **Position**: 5mm inset from each corner
- **Hole Size**: 4.2mm diameter (M4 bolt clearance)
- **Countersink**: 8mm diameter × 3mm deep (M4 socket head cap screw)
- **Thread Option**: M4×0.7 threaded insert pockets (6mm diameter × 8mm deep)

**Cable Management**:
- **Primary Channel**: 8mm × 4mm groove from servo pocket to edge
- **Exit Points**: 3× 6mm diameter holes in rear face
- **Strain Relief**: Gentle 3mm radius curves, no sharp edges

**Material Specifications**:
- **Primary Choice**: PETG (impact resistance, temperature stability)
- **Alternative**: PLA+ (easier printing, adequate strength)
- **Infill**: 40% minimum for structural integrity
- **Wall Thickness**: 3 perimeters minimum (1.2mm)

**Manufacturing Notes**:
- **Print Orientation**: Base down (mounting holes facing print bed)
- **Support Material**: Required for servo pocket overhang
- **Post-Processing**: Servo pocket must be smooth, test-fit actual servo
- **Thread Inserts**: Install while plastic is warm for best retention

### **Segment Links (Segment1.stl & Segment2.stl) - DETAILED SPECIFICATIONS**

#### **Primary Dimensions (CRITICAL - DO NOT MODIFY)**
```c
#define SEGMENT_WIDTH  50.0f   // 50mm × 20mm × 50mm 
#define SEGMENT_HEIGHT 20.0f   
#define SEGMENT_LENGTH 50.0f   // CRITICAL: exactly 50mm for kinematics
```

#### **Detailed Design Requirements**

**Overall Envelope**: 50mm × 20mm × 50mm rectangular beam  
**Quantity Required**: 2 identical pieces (Segment1.stl and Segment2.stl are the same)

**CRITICAL CONSTRAINT**: The 50mm center-to-center distance between servo rotation axes is hardcoded in the trained RL model. **Any deviation will cause complete failure of sim-to-real transfer.**

**Servo Horn Mounting (Proximal End)**:
- **Interface Type**: Standard servo horn attachment
- **Mounting Pattern**: 25-tooth spline, 5.8mm diameter
- **Horn Recess**: 25mm diameter × 3mm deep (standard horn clearance)
- **Center Bolt Access**: 3mm diameter through-hole for horn screw
- **Horn Types**: X-horn, single-arm, or disc (included with MG996R)
- **Alignment**: Horn attachment axis perpendicular to segment length

**Servo Body Mounting (Distal End)**:
- **Servo Pocket**: 41.0mm × 20.0mm × 15.0mm deep
- **Servo Orientation**: Shaft pointing perpendicular to segment length
- **Mounting Tabs**: 2× ears, 10mm spacing, 2.5mm diameter holes
- **Horn Clearance**: 30mm diameter space for horn rotation
- **Shaft Access**: 6mm diameter center hole for wiring passage

**Structural Design**:
- **Wall Thickness**: 2.5mm minimum all around
- **Beam Cross-Section**: 20mm × 50mm (height × width)
- **Internal Geometry**: Hollow with 3mm wall thickness (weight reduction)
- **Stress Concentrations**: 3mm minimum radius on all internal corners

**Wire Management**:
- **Internal Channel**: 6mm × 4mm groove along length
- **Entry/Exit**: Smooth holes at both ends, no sharp edges
- **Capacity**: 3× servo wires + 1× power bus
- **Strain Relief**: Gentle curves, 5mm minimum bend radius

**Manufacturing Requirements**:
- **Tolerance**: ±0.1mm on 50mm length (critical dimension)
- **Surface Finish**: Smooth on bearing surfaces
- **Support Removal**: Complete removal from servo pockets
- **Test Fit**: Every servo must slide in smoothly but without play

**Material Specifications**:
- **Primary**: PLA+ (good strength-to-weight, easy printing)
- **Alternative**: PETG (higher temperature resistance)
- **Infill**: 25% minimum (balance of weight and strength)
- **Print Orientation**: Length parallel to print bed

### **End Effector (End_Cap.stl) - DETAILED SPECIFICATIONS**

#### **Primary Dimensions (CODE-EXACT)**
```c
#define ENDCAP_WIDTH  30.0f   // 30mm × 30mm base
#define ENDCAP_LENGTH 15.0f   // 15mm conical projection
```

#### **Detailed Design Requirements**

**Overall Geometry**: Conical pointing device, 30mm base tapering to fine tip over 15mm length

**Servo Horn Interface (Base)**:
- **Base Diameter**: 30mm (provides good torque distribution)
- **Interface Type**: Standard servo horn spline (25-tooth, 5.8mm)
- **Horn Recess**: 25mm diameter × 3mm deep
- **Mounting Screw**: M3×8mm socket head (standard servo horn screw)
- **Alignment**: Conical tip must be perfectly aligned with servo axis

**Conical Geometry**:
- **Base Diameter**: 30mm
- **Tip Diameter**: 2mm (sharp but printable)
- **Length**: Exactly 15mm (simulation requirement)
- **Taper Angle**: 53.13° (calculated from dimensions)
- **Surface Finish**: Smooth (representing laser beam direction)

**Payload Mounting Options**:

**Option 1: Laser Pointer Mount**
- **Laser Diameter**: 5mm (standard mini laser pointer)
- **Mount Location**: 5mm from tip, centered in cone
- **Mounting Method**: Press-fit or threaded (M5×0.5)
- **Power Routing**: 2mm diameter wire channel to base
- **Activation**: Manual switch or ESP32 GPIO control

**Option 2: Camera Mount**
- **Camera Type**: 5MP OV5640 or similar
- **Mount Location**: Integrated into base section
- **Viewing Angle**: Aligned with pointing direction
- **Cable Management**: Ribbon cable routed through base

**Option 3: Visual Pointer**
- **LED Array**: 3× bright LEDs in triangular pattern
- **Pointing Indication**: LED brightness indicates accuracy
- **Power**: 3.3V from ESP32 GPIO pins

**Precision Requirements**:
- **Tip Alignment**: ±0.5mm maximum deviation from servo axis
- **Balance**: Center of mass within 2mm of servo axis
- **Weight**: <20g maximum (servo torque consideration)
- **Pointing Accuracy**: Mechanical precision ±0.5°

**Manufacturing Specifications**:
- **Material**: PLA (lightweight, fine detail capability)
- **Layer Height**: 0.15mm for smooth conical surface
- **Print Orientation**: Base down, tip up
- **Support**: Tree supports for overhang (remove carefully)
- **Post-Processing**: Sand tip smooth, verify straightness

## 🔧 **SERVO SPECIFICATIONS**

### **Primary Servo: MG996R (3 units required)**
```c
#define NUM_JOINTS 3                    // Base(yaw) + Shoulder(pitch) + Elbow(pitch)
#define JOINT_LIMIT_DEG 180.0f         // 0-180° operating range
#define SERVO_SPEED_DEG_SEC 300.0f     // 300°/sec max speed
#define SERVO_TORQUE_KG_CM 11.0f       // 11 kg⋅cm stall torque
```

**Servo 1 (Base Yaw)**:
- **Function**: Horizontal rotation (0-180°)
- **Mounting**: Vertical axis in base platform center
- **Load**: Entire arm assembly weight
- **Torque Requirement**: HIGH (11 kg⋅cm minimum)

**Servo 2 (Shoulder Pitch)**:
- **Function**: Vertical pitch of first segment (0-180°)
- **Mounting**: Horizontal axis at end of Servo 1
- **Load**: Second segment + end effector
- **Torque Requirement**: MEDIUM (11 kg⋅cm adequate)

**Servo 3 (Elbow Pitch)**:
- **Function**: Vertical pitch of second segment (0-180°)
- **Mounting**: Horizontal axis at end of first segment
- **Load**: End effector only
- **Torque Requirement**: LOW (11 kg⋅cm more than adequate)

### **Alternative Servo Option: SG90 (if weight is critical)**
- **Range**: 0-180° (compatible with code)
- **Torque**: 1.8 kg⋅cm (may be insufficient for base rotation)
- **Weight**: 9g vs 55g for MG996R
- **Recommendation**: Use only for Servo 3 (elbow) if weight reduction needed

## 🏗️ **ASSEMBLY ARCHITECTURE**

### **Kinematic Chain (from simulation code)**
```
Base.stl → Servo1(yaw) → Segment1.stl → Servo2(pitch) → Segment2.stl → Servo3(pitch) → End_Cap.stl
```

### **Physical Assembly Sequence**
1. **Base Assembly**: Mount Servo 1 vertically in base platform center
2. **First Link**: Attach Segment1.stl to Servo 1 horn, mount Servo 2 at far end
3. **Second Link**: Attach Segment2.stl to Servo 2 horn, mount Servo 3 at far end  
4. **End Effector**: Attach End_Cap.stl to Servo 3 horn

### **Critical Alignment Requirements**
- **Servo rotation axes must be precisely perpendicular** to segment lengths
- **50mm center-to-center distance** between servo rotation points (enforced by simulation)
- **Servo horns must be removable** for calibration and maintenance
- **All rotation axes parallel to ground** except Servo 1 (yaw)

## 🎯 **WORKSPACE & PERFORMANCE SPECIFICATIONS**

### **Operating Envelope**
```c
#define WORKSPACE_SIZE 120.0f    // 120mm operational radius
// Maximum reach = 2 * SEGMENT_LENGTH = 100mm
// Workspace margin = 20mm for safety
```
- **Maximum Reach**: 100mm (2 × 50mm segments)
- **Operating Hemisphere**: Forward-facing 180° (simulation constraint)
- **Pointing Accuracy**: <5° angular error (trained performance target)
- **Stability Requirement**: Hold position for 2+ seconds

### **Performance Requirements (from RL training)**
- **Control Frequency**: 50Hz (20ms update cycle)
- **Angular Resolution**: 1° minimum (servo specification)
- **Movement Speed**: Up to 300°/sec (servo limitation)
- **Operating Range**: Each joint 0-180° (no continuous rotation)

## 🔌 **Electronics & Control Interface**

### **Microcontroller: ESP32**
- **Servo Control Pins**: 3 PWM outputs (GPIO 12, 13, 14 recommended)
- **Power Requirements**: 5V/3A minimum for 3× MG996R servos
- **Communication**: WiFi for PC connection (trained model deployment)
- **Programming**: Arduino IDE with Servo library

### **Power System**
- **Servo Power**: 5V DC, 3A capacity (1A per MG996R under load)
- **Logic Power**: 3.3V from ESP32 internal regulator
- **Power Supply**: External 5V/5A wall adapter with barrel jack
- **Safety**: Fuse protection, power switch, LED indicators

### **Wiring Specifications**
```
Servo 1 (Base):    ESP32 GPIO 12 → PWM signal (yellow), 5V (red), GND (brown)
Servo 2 (Shoulder): ESP32 GPIO 13 → PWM signal (yellow), 5V (red), GND (brown)
Servo 3 (Elbow):   ESP32 GPIO 14 → PWM signal (yellow), 5V (red), GND (brown)
```

## 📋 **3D Printing Specifications**

### **Print Settings**
- **Layer Height**: 0.2mm (balance of strength and detail)
- **Infill**: 30% for structural parts, 15% for end effector
- **Print Orientation**: 
  - Base.stl: Flat on print bed (mounting holes down)
  - Segments: Long axis parallel to print bed
  - End_Cap: Base down, tip up
- **Support**: Required for servo mounting pockets

### **Material Recommendations**
- **Base Platform**: PETG (temperature resistance, chemical resistance)
- **Segments**: PLA+ (adequate strength, easy printing)
- **End Effector**: PLA (lightweight, fine detail)
- **Color Coding**: Different colors for each segment (assembly aid)

### **Post-Processing**
- **Servo Mounting Holes**: Drill/ream to exact servo body dimensions
- **Bearing Surfaces**: Sand smooth with 220-grit sandpaper
- **Horn Mounting**: Test-fit servo horns, adjust with fine files
- **Assembly Lubrication**: Light machine oil on bearing surfaces

## 🔧 **Hardware Components List**

### **3D Printed Parts** (STL files required)
- [ ] 1× Base.stl (60×60×20mm)
- [ ] 2× Segment.stl (50×20×50mm) 
- [ ] 1× End_Cap.stl (30×30×15mm conical)

### **Electronic Components**
- [ ] 1× ESP32 development board
- [ ] 3× MG996R servo motors (or SG90 for weight reduction)
- [ ] 1× 5V/5A power supply
- [ ] 1× Breadboard or custom PCB for connections
- [ ] Jumper wires (22 AWG minimum for servo power)

### **Mechanical Hardware**
- [ ] 4× M4×10mm bolts (base mounting)
- [ ] 4× M4 nuts
- [ ] 8× M3×8mm screws (servo mounting)
- [ ] Various servo horns (included with servos)
- [ ] Heat-shrink tubing for wire protection

### **Optional Accessories**
- [ ] Laser pointer module (5mm diameter, 3V)
- [ ] Small camera module (for computer vision applications)
- [ ] LED strips for visual feedback
- [ ] Limit switches for homing (optional)

## 🎛️ **Calibration & Setup Procedure**

### **Mechanical Calibration**
1. **Servo Horn Alignment**: Set all servos to 90° (center position)
2. **Mechanical Zero**: Align segments to known reference positions
3. **Range Verification**: Test full 0-180° motion on each joint
4. **Collision Check**: Verify no mechanical interference throughout range

### **Software Calibration** 
1. **Control Test**: Verify PWM signals control servos correctly
2. **Kinematics Verification**: Compare actual vs. simulated positions
3. **RL Model Deployment**: Load trained model weights from simulation
4. **Performance Validation**: Test pointing accuracy against trained targets

## ⚠️ **Safety & Operation Guidelines**

### **Mechanical Safety**
- **Workspace Clearance**: 150mm radius clear zone during operation
- **Emergency Stop**: Accessible power switch/button
- **Limit Protection**: Software limits prevent servo over-rotation
- **Mounting Security**: Verify base platform securely fastened

### **Electrical Safety**
- **Power Isolation**: Separate servo power from logic power
- **Overcurrent Protection**: Fuse or circuit breaker on servo power
- **Wire Management**: Secure all connections, prevent wire fatigue
- **Heat Management**: Adequate ventilation for servos under continuous operation

## 📊 **Expected Performance Metrics**

Based on RL training results, the completed hardware should achieve:

- **Pointing Accuracy**: <5° angular error consistently
- **Stability**: Hold target position ±1° for 2+ seconds
- **Response Time**: <200ms from command to position
- **Workspace Coverage**: 100mm radius, 180° horizontal sweep
- **Operating Speed**: Up to 300°/sec peak movement
- **Power Consumption**: <15W continuous operation

## 📝 **3D Modeling Deliverables Required**

### **Primary STL Files** (dimensionally accurate)
1. **Base.stl**: 60×60×20mm platform with servo mounting
2. **Segment.stl**: 50×20×50mm link (2 copies needed)
3. **End_Cap.stl**: 30×30×15mm conical tip

### **Assembly Documentation**
1. **Exploded View Drawing**: Show all parts and assembly sequence
2. **Servo Mounting Details**: Precise servo body fitment
3. **Hardware Specification**: All bolts, screws, and fasteners
4. **Wire Routing Plan**: Cable management and protection

### **Manufacturing Notes**
1. **Tolerance Specifications**: ±0.1mm on critical dimensions
2. **Surface Finish Requirements**: Smooth bearing surfaces
3. **Print Orientation Guidance**: Optimal strength and surface finish
4. **Support Material Locations**: Minimize post-processing

---

## 🚀 **Project Success Criteria**

**Hardware Completion**: All parts printed, assembled, and mechanically functional  
**Electronics Integration**: ESP32 controlling all 3 servos reliably  
**Software Deployment**: Trained RL model running on hardware  
**Performance Validation**: Achieving <5° pointing accuracy as trained  
**Documentation**: Complete assembly instructions and operation manual

This specification ensures the physical hardware exactly matches the digital twin used for RL training, maximizing the success of sim-to-real transfer.