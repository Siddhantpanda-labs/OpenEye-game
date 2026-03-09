-- CharacterController sample (Script_advice.md Phase 2).
-- Uses Input.GetKey / GetKeyDown and World.SetVelocity for WASD movement and jump.
-- Optional: Input.GetMouseDelta() for look (yaw); requires entity to have RigidBody for velocity.

local CharacterController = {}

-- Schema: editor shows these as typed fields. Types: number, string, boolean, entity, vec3
CharacterController.schema = {
    { name = "moveSpeed", type = "number", default = 5.0, min = 0, max = 50 },
    { name = "jumpSpeed", type = "number", default = 7.0, min = 0, max = 30 },
    { name = "sensitivity", type = "number", default = 0.002, min = 0.0001, max = 0.1 },
}

function CharacterController:OnStart()
    self.moveSpeed = self.properties.moveSpeed and self.properties.moveSpeed or 5.0
    self.jumpSpeed = self.properties.jumpSpeed and self.properties.jumpSpeed or 7.0
    self.sensitivity = self.properties.sensitivity and self.properties.sensitivity or 0.002
    self.yaw = 0.0
end

function CharacterController:OnUpdate(deltaTime)
    local vx, vy, vz = 0.0, 0.0, 0.0

    -- WASD movement (world XZ plane)
    if Input.GetKey("W") then vz = vz - self.moveSpeed end
    if Input.GetKey("S") then vz = vz + self.moveSpeed end
    if Input.GetKey("A") then vx = vx - self.moveSpeed end
    if Input.GetKey("D") then vx = vx + self.moveSpeed end

    -- Jump (one-shot via GetKeyDown would need ground check; here we use key-held for demo)
    if Input.GetKeyDown("Space") then
        vy = self.jumpSpeed
    end

    World.SetVelocity(self.entity, vx, vy, vz)

    -- Mouse look: accumulate yaw from horizontal delta
    local dx, dy = Input.GetMouseDelta()
    self.yaw = self.yaw - dx * self.sensitivity
    -- Apply Y rotation only (simplified: quat from yaw around Y)
    local c, s = math.cos(self.yaw * 0.5), math.sin(self.yaw * 0.5)
    World.SetRotation(self.entity, c, 0.0, s, 0.0)  -- w, x, y, z for Y-axis rotation
end

function CharacterController:OnDestroy()
end

return CharacterController
