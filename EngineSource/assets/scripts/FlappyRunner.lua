-- Flappy-style runner: forward velocity, Space to flap up, reset when touching obstacles.
-- Uses physics collision: entities with Collider (+ optional RigidBody) that touch the player
-- are reported via World.GetCollisions. Tag obstacles with obstacleTag (default "Obstacle").
-- Assign startPositionEntity in the inspector (e.g. an empty entity at spawn).
-- Entity needs RigidBody + Collider for physics and collision.

local FlappyRunner = {}

FlappyRunner.schema = {
    { name = "startPositionEntity", type = "entity", default = nil },
    { name = "forwardSpeed", type = "number", default = 8.0, min = 0, max = 50 },
    { name = "flapStrength", type = "number", default = 6.0, min = 0, max = 30 },
    { name = "obstacleTag", type = "string", default = "Obstacle" },
    { name = "gravity", type = "number", default = -15.0, min = -50, max = 0 },
}

function FlappyRunner:OnStart()
    self.forwardSpeed = self.properties.forwardSpeed or 8.0
    self.flapStrength = self.properties.flapStrength or 6.0
    self.obstacleTag = (self.properties.obstacleTag and self.properties.obstacleTag ~= "") and self.properties.obstacleTag or "Obstacle"
    self.gravity = self.properties.gravity or -15.0

    -- Resolve start position from startPositionEntity or self
    local startEnt = self.properties.startPositionEntity
    if startEnt and type(startEnt) == "table" and startEnt.index then
        local p = World.GetPosition(startEnt)
        if p then
            self.startX, self.startY, self.startZ = p.x, p.y, p.z
        end
    end
    if not self.startX then
        local p = World.GetPosition(self.entity)
        self.startX = p and p.x or 0
        self.startY = p and p.y or 0
        self.startZ = p and p.z or 0
    end
end

function FlappyRunner:OnUpdate(deltaTime)
    -- Physics collision: respawn if we touched any entity with obstacleTag
    local collisions = World.GetCollisions(self.entity)
    if collisions then
        for i = 1, #collisions do
            local other = collisions[i]
            if other and World.HasTag(other, self.obstacleTag) then
                World.Teleport(self.entity, self.startX, self.startY, self.startZ)
                World.SetVelocity(self.entity, 0, 0, -self.forwardSpeed)
                return
            end
        end
    end

    local vel = World.GetVelocity(self.entity)
    local vx = vel and vel.x or 0.0
    local vy = vel and vel.y or 0.0
    local vz = vel and vel.z or 0.0

    -- Forward (e.g. -Z)
    vz = -self.forwardSpeed
    -- Gravity
    vy = vy + self.gravity * deltaTime
    -- Flap
    if Input.GetKeyDown("Space") then
        vy = self.flapStrength
    end

    World.SetVelocity(self.entity, vx, vy, vz)

    -- Keep entity upright: no rotation (identity) and no spin
    World.SetAngularVelocity(self.entity, 0, 0, 0)
    local p = World.GetPosition(self.entity)
    if p then
        World.Teleport(self.entity, p.x, p.y, p.z, { w = 1, x = 0, y = 0, z = 0 })
    end
end

function FlappyRunner:OnDestroy()
end

return FlappyRunner
