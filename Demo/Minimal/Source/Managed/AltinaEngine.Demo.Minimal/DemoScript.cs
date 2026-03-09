using System;
using AltinaEngine.Managed;

namespace AltinaEngine.Demo.Minimal;

public sealed class DemoScript : ScriptComponent
{
    private float _elapsedSeconds;
    private bool _loggedCreate;
    private bool _loggedFirstTick;
    private const float MoveSpeed = 24.0f;

    public override void OnCreate()
    {
        if (_loggedCreate)
        {
            return;
        }

        _loggedCreate = true;
        ManagedLog.Info($"[DemoScript] OnCreate owner=({OwnerIndex},{OwnerGeneration}) world={WorldId}");
    }

    public override void OnDestroy()
    {
        ManagedLog.Info("[DemoScript] OnDestroy");
    }

    public override void Tick(float dt)
    {
        if (!_loggedFirstTick)
        {
            _loggedFirstTick = true;
            ManagedLog.Info("[DemoScript] Tick start.");
        }

        _elapsedSeconds += dt;
        if (_elapsedSeconds >= 1.0f)
        {
            _elapsedSeconds = 0.0f;
            ManagedLog.Info($"[DemoScript] Tick mouse=({Input.MouseX},{Input.MouseY})");
        }

        if (!Input.HasFocus)
        {
            return;
        }

        if (Input.WasKeyPressed(EKey.Space))
        {
            ManagedLog.Info("[DemoScript] Space pressed (managed).");
        }

        if (!TryGetWorldPosition(out var position))
        {
            return;
        }

        float moveX = 0.0f;
        float moveY = 0.0f;
        float moveZ = 0.0f;
        if (Input.IsKeyDown(EKey.W))
        {
            moveZ += 1.0f;
        }
        if (Input.IsKeyDown(EKey.S))
        {
            moveZ -= 1.0f;
        }
        if (Input.IsKeyDown(EKey.A))
        {
            moveX -= 1.0f;
        }
        if (Input.IsKeyDown(EKey.D))
        {
            moveX += 1.0f;
        }
        if (Input.IsKeyDown(EKey.Q))
        {
            moveY -= 1.0f;
        }
        if (Input.IsKeyDown(EKey.E))
        {
            moveY += 1.0f;
        }
        if (moveX != 0.0f || moveY != 0.0f || moveZ != 0.0f)
        {
            float length = MathF.Sqrt(moveX * moveX + moveY * moveY + moveZ * moveZ);
            if (length > 0.0f)
            {
                moveX /= length;
                moveY /= length;
                moveZ /= length;
            }

            position.X += moveX * MoveSpeed * dt;
            position.Y += moveY * MoveSpeed * dt;
            position.Z += moveZ * MoveSpeed * dt;
            TrySetWorldPosition(position);
        }
    }
}
