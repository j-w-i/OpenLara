R"====(
#if defined(ALPHA_TEST) && !defined(NANO_SHADOW)
	varying vec2 vTexCoord;
#endif
#ifdef NANO_SHADOW
	// iPod nano 7 (MBX Lite): no depth textures. Casters write half their linear light-space depth
	// (orthographic light) as a colour, T = depth / 2; receivers (NANO_SHADOW_RECV) compare it per
	// pixel in the fixed combiner, two stages:
	//   s = clamp(4(T + 1 - R - 0.5)),  out = clamp(4(s + A - 0.5))
	// multiply-blended over the lit scene. R (.xyz) = receiver depth / 2 + uMaterial.x, A (.w) per
	// vertex. That is a x16 slope on T - R with a floor of 4A - 2: lit where the map's depth is at
	// least the receiver's (minus a small bias), fully shadowed 1/16 of the depth range further.
	// Half depth keeps R + offset below 1 over the whole light box (a vertex colour clamps there).
	// The floor is what compose.glsl leaves of a shadowed room pixel, mix(min(ambient, baked), baked,
	// shadow + (1 - shadow) * fade): keep = ambient / baked light (uMaterial.y = room ambient), raised
	// toward 1 by fade = squared distance from the main light over its radius (uParam = light position,
	// 1 / radius), as getShadow() does. Past the far end of the light box (uMaterial.zw) it is lit.
	// A = (2 + floor) / 4.
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
		uniform vec4 uParam;
		attribute vec4 aLight;
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
				float keep = min(uMaterial.y / max(aLight.y, 0.004), NANO_SHADOW_KEEP_MAX);   // green stands for the baked brightness; never divide by 0
				vec3 lv = (uParam.xyz - coord) * uParam.w;
				keep += (1.0 - keep) * min(dot(lv, lv), 1.0);               // weaker toward the edge of the light's reach
				keep = max(keep, lp.z * uMaterial.z + uMaterial.w);          // lit past the far end of the light box
				vColor = vec4(lp.zzz * 0.5 + uMaterial.xxx, keep * 0.25 + 0.5);
			#else
				vColor = vec4(cpos.z * 0.25 + 0.25);
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
				fragColor = vec4(clamp(((s + vColor.www) - 0.5) * 4.0, 0.0, 1.0), 1.0);
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
