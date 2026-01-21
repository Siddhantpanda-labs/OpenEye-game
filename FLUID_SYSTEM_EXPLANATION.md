# Fluid System Explanation

## Overview
The fluid system implements a **3D grid-based fluid simulation** using GPU compute shaders. It simulates both **gas** (smoke/fire) and **liquid** (water) fluids using Navier-Stokes equations.

## How It Works

### 1. **Simulation Grid**
- The fluid volume is divided into a **3D grid** (e.g., 64×64×64 voxels)
- Each voxel stores:
  - **Density** (how much fluid is present)
  - **Velocity** (direction and speed of fluid movement)
  - **Temperature** (for gas - affects buoyancy)
  - **Pressure** (for incompressibility)

### 2. **Simulation Steps** (per frame)
1. **Emission**: New density/temperature is injected at the emission point
2. **Advection**: Density and velocity move through the grid based on velocity field
3. **Wind Forces**: External forces (wind) are applied
4. **Pressure Projection**: Ensures fluid is incompressible (divergence-free)
5. **Buoyancy**: Hot gas rises due to temperature differences
6. **Dissipation**: Density and velocity gradually fade over time

### 3. **Rendering** (Raymarching)
The fluid is rendered using **volumetric raymarching**:
1. **Ray Data Pass**: Calculates where rays enter/exit the fluid volume
2. **Raymarching Pass**: 
   - Casts rays through the 3D density texture
   - Samples density at regular intervals
   - Accumulates color and alpha based on density
   - For **gas**: Semi-transparent smoke with lighting
   - For **liquid**: Surface-based rendering with refraction

## Fluid Behavior

### **Gas (Smoke/Fire)**
- **Rises upward** due to buoyancy (hot air rises)
- **Dissipates** over time (density decreases)
- **Spreads** outward from emission point
- **Affected by wind** (if enabled)
- **Semi-transparent** - you can see through it

### **Liquid (Water)**
- **Falls downward** due to gravity
- **Surface-based** - rendered as a surface, not volume
- **Refracts** light (bends light rays)
- **Less dissipation** - more persistent

## Key Parameters

### **Emission**
- **Position**: Where fluid is emitted (normalized [0,1] in volume)
- **Radius**: Size of emission area
- **Density Rate**: How much density is added per second
- **Temperature Rate**: How much heat is added (for gas)

### **Buoyancy**
- **Sigma (σ)**: Buoyancy strength (higher = rises faster)
- **Kappa (κ)**: Weight factor (lower = lighter, rises more)
- **Ambient Temperature**: Base temperature of environment

### **Dissipation**
- **Density Dissipation**: How fast density fades (0.99 = 1% per frame)
- **Velocity Dissipation**: How fast velocity decays

### **Wind**
- **Direction**: Wind direction vector (normalized)
- **Strength**: Wind force magnitude
- **Enabled**: Toggle wind on/off

## Why You Might Not See Fluids

1. **No Density**: If emission is disabled or density rate is too low, no fluid appears
2. **Wrong Position**: Camera might not be looking at the fluid volume
3. **Too Small**: Volume size might be too small to see
4. **Too Transparent**: Density might be too low (increase density rate)
5. **Dissipation Too High**: Fluid disappears too quickly
6. **Emission Position**: Emission point might be outside visible area

## Tips for Better Visibility

1. **Increase Density Rate**: Set `densityRate` to 1.0-2.0 for gas
2. **Increase Volume Size**: Make volume larger (e.g., 3×3×3 units)
3. **Position Camera**: Look at the fluid volume location
4. **Reduce Dissipation**: Lower `densityDissipation` (e.g., 0.995 instead of 0.99)
5. **Enable Emission**: Make sure `m_emitEnabled = true`
6. **Check Resolution**: Higher resolution (128×128×128) = better quality but slower

## Presets

### **Fire/Smoke**
- High density rate (1.2)
- High temperature (2.0)
- Strong buoyancy (rises quickly)
- Wind enabled (spreads outward)

### **Water**
- Medium density rate (0.8)
- No temperature (liquid)
- Lower buoyancy
- No wind (falls straight down)

### **Explosion**
- Very high density rate (2.0)
- Very high temperature (3.0)
- Strong buoyancy
- Strong upward wind (explosive force)

## Technical Details

- **Grid Resolution**: Controls simulation quality (higher = better but slower)
- **Volume Size**: World-space size of the fluid volume
- **Raymarching Steps**: Number of samples along each ray (affects quality)
- **Blending**: Uses alpha blending for transparency
- **Depth Testing**: Disabled during raymarching (composited afterward)
