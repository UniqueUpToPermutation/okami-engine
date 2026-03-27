#version 410 core

#include "scene.glsl"

#define VertexData \
	_VertexData { \
		noperspective float m_edgeDistance; \
		noperspective float m_size; \
		smooth vec4 m_color; \
	}

#define kAntialiasing 2.0

#ifdef VERTEX_SHADER
	#include "im3d_interface.glsl"

	layout(std140) uniform SceneGlobalsBlock {
		SceneGlobals sceneGlobals;
	};

	out VertexData vData;

	void main() 
	{
		vData.m_color = a_color.wzyx; // swizzle to correct endianness
		#if !defined(TRIANGLES)
			vData.m_color.a *= smoothstep(0.0, 1.0, a_positionSize.w / kAntialiasing);
		#endif
		vData.m_size = max(a_positionSize.w, kAntialiasing);
		gl_Position = sceneGlobals.u_camera.u_viewProj * vec4(a_positionSize.xyz, 1.0);
	}
#endif

#ifdef GEOMETRY_SHADER
	layout(std140) uniform SceneGlobalsBlock {
		SceneGlobals sceneGlobals;
	};

#if defined(POINTS)
	// expand point -> screen-aligned quad
	layout(points) in;
	layout(triangle_strip, max_vertices = 4) out;

	in  VertexData vData[];
	out VertexData vDataOut;
	noperspective out vec2 v_localCoord;

	void main()
	{
		vec2 viewport = sceneGlobals.u_camera.u_viewport.xy;
		float sz = vData[0].m_size;
		vec2 halfNDC = vec2(sz) / viewport;
		vec4 pos = gl_in[0].gl_Position;

		// Emit a screen-aligned quad: offsets in [-1,1] x [-1,1] local space
		vec2 offsets[4] = vec2[](
			vec2(-1.0, -1.0),
			vec2( 1.0, -1.0),
			vec2(-1.0,  1.0),
			vec2( 1.0,  1.0)
		);
		for (int i = 0; i < 4; ++i) {
			gl_Position = vec4(pos.xy + offsets[i] * halfNDC * pos.w, pos.zw);
			vDataOut.m_edgeDistance = 0.0;
			vDataOut.m_size = sz;
			vDataOut.m_color = vData[0].m_color;
			v_localCoord = offsets[i];
			EmitVertex();
		}
		EndPrimitive();
	}

#elif defined(LINES)
	// expand line -> triangle strip
	layout(lines) in;
	layout(triangle_strip, max_vertices = 4) out;

	in  VertexData vData[];
	out VertexData vDataOut;

	void main() 
	{
		vec2 pos0 = gl_in[0].gl_Position.xy / gl_in[0].gl_Position.w;
		vec2 pos1 = gl_in[1].gl_Position.xy / gl_in[1].gl_Position.w;

		vec2 viewport = sceneGlobals.u_camera.u_viewport.xy;
		vec2 dir = pos0 - pos1;
		dir = normalize(vec2(dir.x, dir.y * viewport.y / viewport.x)); // correct for aspect ratio
		vec2 tng0 = vec2(-dir.y, dir.x);
		vec2 tng1 = tng0 * vData[1].m_size / viewport;
		tng0 = tng0 * vData[0].m_size / viewport;

		// line start
		gl_Position = vec4((pos0 - tng0) * gl_in[0].gl_Position.w, gl_in[0].gl_Position.zw); 
		vDataOut.m_edgeDistance = -vData[0].m_size;
		vDataOut.m_size = vData[0].m_size;
		vDataOut.m_color = vData[0].m_color;
		EmitVertex();

		gl_Position = vec4((pos0 + tng0) * gl_in[0].gl_Position.w, gl_in[0].gl_Position.zw);
		vDataOut.m_color = vData[0].m_color;
		vDataOut.m_edgeDistance = vData[0].m_size;
		vDataOut.m_size = vData[0].m_size;
		EmitVertex();

		// line end
		gl_Position = vec4((pos1 - tng1) * gl_in[1].gl_Position.w, gl_in[1].gl_Position.zw);
		vDataOut.m_edgeDistance = -vData[1].m_size;
		vDataOut.m_size = vData[1].m_size;
		vDataOut.m_color = vData[1].m_color;
		EmitVertex();

		gl_Position = vec4((pos1 + tng1) * gl_in[1].gl_Position.w, gl_in[1].gl_Position.zw);
		vDataOut.m_color = vData[1].m_color;
		vDataOut.m_size = vData[1].m_size;
		vDataOut.m_edgeDistance = vData[1].m_size;
		EmitVertex();
	}
#endif
#endif

#ifdef FRAGMENT_SHADER
	in VertexData vData;
	#if defined(POINTS)
	noperspective in vec2 v_localCoord;
	#endif

	layout(location=0) out vec4 fResult;

	void main() 
	{
		fResult = vData.m_color;

		#if defined(LINES)
			float d = abs(vData.m_edgeDistance) / vData.m_size;
			d = smoothstep(1.0, 1.0 - (kAntialiasing / vData.m_size), d);
			fResult.a *= d;

		#elif defined(POINTS)
			// v_localCoord is in [-1,1]x[-1,1]; length=1 is the circle edge
			float d = length(v_localCoord);
			d = smoothstep(1.0, 1.0 - (kAntialiasing * 2.0 / vData.m_size), d);
			fResult.a *= d;

		#endif

		if (fResult.a < 0.001)
			discard;
	}
#endif