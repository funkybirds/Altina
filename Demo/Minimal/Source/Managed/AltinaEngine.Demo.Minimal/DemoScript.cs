using System;
using AltinaEngine.Managed;

namespace AltinaEngine.Demo.Minimal;

public sealed class DemoScript : ScriptComponent
{
    private float _elapsedSeconds;
    private bool _loggedCreate;
    private bool _loggedFirstTick;
    private const float MoveSpeed = 2.5f;

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
        if (Input.IsKeyDown(EKey.W))
        {
            moveY += 1.0f;
        }
        if (Input.IsKeyDown(EKey.S))
        {
            moveY -= 1.0f;
        }
        if (Input.IsKeyDown(EKey.A))
        {
            moveX -= 1.0f;
        }
        if (Input.IsKeyDown(EKey.D))
        {
            moveX += 1.0f;
        }

        if (moveX != 0.0f || moveY != 0.0f)
        {
            float length = MathF.Sqrt(moveX * moveX + moveY * moveY);
            if (length > 0.0f)
            {
                moveX /= length;
                moveY /= length;
            }

            position.X += moveX * MoveSpeed * dt;
            position.Y += moveY * MoveSpeed * dt;
            TrySetWorldPosition(position);
        }
    }
}
