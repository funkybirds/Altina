using System;
using AltinaEngine.Managed;

namespace AltinaEngine.Demo.SpaceshipGame;

public sealed class ShipFreeMove : ScriptComponent
{
    private const float MoveSpeed = 12.0f;

    public override void Tick(float dt)
    {
        if (!TryGetWorldPosition(out var position))
        {
            return;
        }

        float moveX = 0.0f;
        float moveY = 0.0f;
        float moveZ = 0.0f;

        // World-space axes: X=right, Y=up, Z=forward.
        if (Input.IsKeyDown(EKey.W)) moveZ += 1.0f;
        if (Input.IsKeyDown(EKey.S)) moveZ -= 1.0f;
        if (Input.IsKeyDown(EKey.A)) moveX -= 1.0f;
        if (Input.IsKeyDown(EKey.D)) moveX += 1.0f;
        if (Input.IsKeyDown(EKey.Q)) moveY -= 1.0f;
        if (Input.IsKeyDown(EKey.E)) moveY += 1.0f;

        if (moveX == 0.0f && moveY == 0.0f && moveZ == 0.0f)
        {
            return;
        }

        float len = MathF.Sqrt(moveX * moveX + moveY * moveY + moveZ * moveZ);
        if (len > 1e-6f)
        {
            moveX /= len;
            moveY /= len;
            moveZ /= len;
        }

        position.X += moveX * MoveSpeed * dt;
        position.Y += moveY * MoveSpeed * dt;
        position.Z += moveZ * MoveSpeed * dt;
        TrySetWorldPosition(position);
    }
}

