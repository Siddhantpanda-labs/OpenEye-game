-- Simple test script for Phase 1
-- This script will be attached to an entity and executed

local TestScript = {}

function TestScript:OnStart()
    print("TestScript: OnStart called!")
    self.counter = 0
end

function TestScript:OnUpdate(deltaTime)
    self.counter = self.counter + deltaTime
    
    if self.counter >= 1.0 then
        print(string.format("TestScript: Update called! Time: %.2f", self.counter))
        self.counter = 0
    end
end

function TestScript:OnDestroy()
    print("TestScript: OnDestroy called!")
end

return TestScript
