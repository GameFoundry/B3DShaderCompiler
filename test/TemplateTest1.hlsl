// xsc-args: -Xhlsl-templates ON

template<typename T>
struct Pair
{
    T left;
    T right;
};

template<typename T>
T Add(T lhs, T rhs)
{
    return lhs + rhs;
}

template<>
float4 Add(float4 lhs, float4 rhs)
{
    return lhs - rhs;
}

template<>
struct Pair<uint>
{
    uint left;
    uint right;
};

template<typename V, typename T>
V Convert(T value)
{
    return (V)value;
}

template<typename T>
T First(Pair<T> value)
{
    return value.left;
}

float4 VS(float4 position : POSITION) : SV_Position
{
    Pair<float4> values;
    values.left = position;
    values.right = float4(1.0, 2.0, 3.0, 4.0);

    uint offset = Convert<uint>(1.0f);
    Pair<uint> integers;
    integers.left = offset;
    integers.right = Add(offset, 1u);

    Pair<Pair<float4>> nested;
    nested.left = values;
    nested.right = values;

    return Add(First(values), values.right) + float4((float)integers.right, 0.0, 0.0, 0.0);
}
