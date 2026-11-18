R"====(
#if defined(ALPHA_TEST) && !defined(NANO_SHADOW)
	varying vec2 vTexCoord;
#endif
#ifdef NANO_SHADOW
	// iPod nano 7 (MBX Lite): no depth textures or per-pixel maths. Casters write their linear
	// light-space depth (orthographic light) as a colour; receivers (NANO_SHADOW_RECV) compare it
	// in the fixed combiner: two stages, s = clamp(4(T + 1 - R - 0.5)), out = s*K + (1-K),
	// multiply-blended over the lit scene. R (.xyz) = receiver depth + uMaterial.x, K (.w) = uMaterial.y.
	varying vec4 vColor;
	#ifdef NANO_SHADOW_RECV
		varying vec2 vTexCoord;
	#endif
#endif

#ifdef VERTEX

	uniform mat4 uViewProj;
	uniform vec4 uBasis[32 * 2];

	attribute vec4 aCoord;
	#if defined(ALPHA_TEST) && !defined(NANO_SHADOW)
		attribute vec4 aTexCoord;
	#endif
	#ifdef NANO_SHADOW_RECV
		uniform mat4 uLightProj;
		uniform vec4 uMaterial;
	#endif

	vec3 mulQuat(vec4 q, vec3 v) {
		return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + v * q.w);
	}

	vec3 mulBasis(vec4 rot, vec3 pos, vec3 v) {
		return mulQuat(rot, v) + pos;
	}

	void main() {
		#ifdef MESH_SKINNING
			int index = int(aCoord.w);
			vec4 rBasisRot = uBasis[index];
			vec4 rBasisPos = uBasis[index + 1];
		#else
			vec4 rBasisRot = uBasis[0];
			vec4 rBasisPos = uBasis[1];
		#endif
		#if defined(ALPHA_TEST) && !defined(NANO_SHADOW)
			vTexCoord = aTexCoord.xy;
		#endif
		vec3 coord  = mulBasis(rBasisRot, rBasisPos.xyz, aCoord.xyz);
		vec4 cpos = uViewProj * vec4(coord, rBasisPos.w);
		gl_Position = cpos;
		#ifdef NANO_SHADOW
			#ifdef NANO_SHADOW_RECV
				vec4 lp = uLightProj * vec4(coord, 1.0);
				vTexCoord = lp.xy;
				vColor = vec4(lp.zzz + uMaterial.xxx, uMaterial.y);
			#else
				vColor = vec4(cpos.z * 0.5 + 0.5);
			#endif
		#endif
	}
	
#else
	
	#if (defined(ALPHA_TEST) && !defined(NANO_SHADOW)) || defined(NANO_SHADOW_RECV)
		uniform sampler2D sDiffuse;
	#endif

	#ifndef NANO_SHADOW
	vec4 pack(float value) {
		vec4 v = fract(value * vec4(1.0, 255.0, 65025.0, 16581375.0));
		return v - v.yzww * vec4(1.0/255.0, 1.0/255.0, 1.0/255.0, 0.0);
	}
	#endif

	void main() {
		#ifdef NANO_SHADOW
			#ifdef NANO_SHADOW_RECV
				vec4 t = texture2D(sDiffuse, vTexCoord);
				vec3 s = clamp(((t.xyz + (1.0 - vColor.xyz)) - 0.5) * 4.0, 0.0, 1.0);
				fragColor = vec4(s * vColor.www + (1.0 - vColor.www), 1.0);
			#else
				fragColor = vColor;
			#endif
		#else
		#ifdef ALPHA_TEST
			if (texture2D(sDiffuse, vTexCoord).w <= 0.5)
				discard;
		#endif

		#ifdef SHADOW_COLOR
			fragColor = pack(gl_FragCoord.z);
		#else
			fragColor = vec4(1.0);
		#endif
		#endif
	}

#endif
)===="
