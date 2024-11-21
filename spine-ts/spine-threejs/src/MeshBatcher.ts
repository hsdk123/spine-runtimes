/******************************************************************************
 * Spine Runtimes License Agreement
 * Last updated July 28, 2023. Replaces all prior versions.
 *
 * Copyright (c) 2013-2023, Esoteric Software LLC
 *
 * Integration of the Spine Runtimes into software or otherwise creating
 * derivative works of the Spine Runtimes is permitted under the terms and
 * conditions of Section 2 of the Spine Editor License Agreement:
 * http://esotericsoftware.com/spine-editor-license
 *
 * Otherwise, it is permitted to integrate the Spine Runtimes into software or
 * otherwise create derivative works of the Spine Runtimes (collectively,
 * "Products"), provided that each user of the Products must obtain their own
 * Spine Editor license and redistribution of the Products in any form must
 * include this license and copyright notice.
 *
 * THE SPINE RUNTIMES ARE PROVIDED BY ESOTERIC SOFTWARE LLC "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL ESOTERIC SOFTWARE LLC BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES,
 * BUSINESS INTERRUPTION, OR LOSS OF USE, DATA, OR PROFITS) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THE
 * SPINE RUNTIMES, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/

import * as THREE from "three"

import { ThreeJsTexture, ThreeBlendOptions } from "./ThreeJsTexture.js";
import { BlendMode } from "@esotericsoftware/spine-core";
import { SkeletonMesh } from "./SkeletonMesh.js";

type MaterialWithMap = THREE.Material & { map: THREE.Texture | null };
export class MeshBatcher extends THREE.Mesh {
	public static MAX_VERTICES = 10920;

	// private static VERTEX_SIZE = 9;
	private static VERTEX_SIZE = 13;
	private vertexSize = 9;
	private vertexBuffer: THREE.InterleavedBuffer;
	private vertices: Float32Array;
	private verticesLength = 0;
	private indices: Uint16Array;
	private indicesLength = 0;
	private materialGroups: [number, number, number][] = [];

	constructor (
			maxVertices: number = MeshBatcher.MAX_VERTICES,
			private materialFactory: (parameters: THREE.MaterialParameters) => THREE.Material,
			private twoColorTint = true,
		) {
		super();
		if (maxVertices > MeshBatcher.MAX_VERTICES) throw new Error("Can't have more than 10920 triangles per batch: " + maxVertices);

		if (twoColorTint) {
			this.vertexSize += 4;
		}

		let vertices = this.vertices = new Float32Array(maxVertices * this.vertexSize);
		let indices = this.indices = new Uint16Array(maxVertices * 3);
		let geo = new THREE.BufferGeometry();
		let vertexBuffer = this.vertexBuffer = new THREE.InterleavedBuffer(vertices, this.vertexSize);
		vertexBuffer.usage = WebGLRenderingContext.DYNAMIC_DRAW;
		geo.setAttribute("position", new THREE.InterleavedBufferAttribute(vertexBuffer, 3, 0, false));
		geo.setAttribute("color", new THREE.InterleavedBufferAttribute(vertexBuffer, 4, 3, false));
		geo.setAttribute("uv", new THREE.InterleavedBufferAttribute(vertexBuffer, 2, 7, false));
		if (twoColorTint) {
			geo.setAttribute("darkcolor", new THREE.InterleavedBufferAttribute(vertexBuffer, 4, 9, false));
		}
		geo.setIndex(new THREE.BufferAttribute(indices, 1));
		geo.getIndex()!.usage = WebGLRenderingContext.DYNAMIC_DRAW;
		geo.drawRange.start = 0;
		geo.drawRange.count = 0;
		this.geometry = geo;
		this.material = [];
	}

	dispose () {
		this.geometry.dispose();
		if (this.material instanceof THREE.Material)
			this.material.dispose();
		else if (this.material) {
			for (let i = 0; i < this.material.length; i++) {
				let material = this.material[i];
				if (material instanceof THREE.Material)
					material.dispose();
			}
		}
	}

	clear () {
		let geo = (<THREE.BufferGeometry>this.geometry);
		geo.drawRange.start = 0;
		geo.drawRange.count = 0;
		geo.clearGroups();
		this.materialGroups = [];
		if (this.material instanceof THREE.Material) {
			const meshMaterial = this.material as MaterialWithMap;
			meshMaterial.map = null;
			meshMaterial.blending = THREE.NormalBlending;
		} else if (Array.isArray(this.material)) {
			for (let i = 0; i < this.material.length; i++) {
				const meshMaterial = this.material[i] as MaterialWithMap;
				meshMaterial.map = null;
				meshMaterial.blending = THREE.NormalBlending;
			}
		}
		return this;
	}

	begin () {
		this.verticesLength = 0;
		this.indicesLength = 0;
	}

	canBatch (numVertices: number, numIndices: number) {
		if (this.indicesLength + numIndices >= this.indices.byteLength / 2) return false;
		if (this.verticesLength / this.vertexSize + numVertices >= (this.vertices.byteLength / 4) / this.vertexSize) return false;
		return true;
	}

	batch (vertices: ArrayLike<number>, verticesLength: number, indices: ArrayLike<number>, indicesLength: number, z: number = 0) {
		let indexStart = this.verticesLength / this.vertexSize;
		let vertexBuffer = this.vertices;
		let i = this.verticesLength;
		let j = 0;
		if (this.twoColorTint) {
			for (; j < verticesLength;) {
				vertexBuffer[i++] = vertices[j++];  // x
				vertexBuffer[i++] = vertices[j++];  // y
				vertexBuffer[i++] = z;				// z

				vertexBuffer[i++] = vertices[j++];  // r
				vertexBuffer[i++] = vertices[j++];  // g
				vertexBuffer[i++] = vertices[j++];  // b
				vertexBuffer[i++] = vertices[j++];  // a

				vertexBuffer[i++] = vertices[j++];  // u
				vertexBuffer[i++] = vertices[j++];  // v

				vertexBuffer[i++] = vertices[j++];  // r
				vertexBuffer[i-1] = 1;
				vertexBuffer[i++] = vertices[j++];  // g
				vertexBuffer[i-1] = 1;
				vertexBuffer[i++] = vertices[j++];  // b
				vertexBuffer[i-1] = 1;
				vertexBuffer[i++] = vertices[j++];  // a
				vertexBuffer[i-1] = 1;


			}
		} else {
			for (; j < verticesLength;) {
				vertexBuffer[i++] = vertices[j++];  // x
				vertexBuffer[i++] = vertices[j++];  // y
				vertexBuffer[i++] = z;				// z

				vertexBuffer[i++] = vertices[j++];  // r
				vertexBuffer[i++] = vertices[j++];  // g
				vertexBuffer[i++] = vertices[j++];  // b
				vertexBuffer[i++] = vertices[j++];  // a

				vertexBuffer[i++] = vertices[j++];  // u
				vertexBuffer[i++] = vertices[j++];  // v
			}
		}
		this.verticesLength = i;

		let indicesArray = this.indices;
		for (i = this.indicesLength, j = 0; j < indicesLength; i++, j++)
			indicesArray[i] = indices[j] + indexStart;
		this.indicesLength += indicesLength;
	}

	end () {
		this.vertexBuffer.needsUpdate = this.verticesLength > 0;
		this.vertexBuffer.addUpdateRange(0, this.verticesLength);
		let geo = (<THREE.BufferGeometry>this.geometry);
		this.closeMaterialGroups();
		let index = geo.getIndex();
		if (!index) throw new Error("BufferAttribute must not be null.");
		index.needsUpdate = this.indicesLength > 0;
		index.addUpdateRange(0, this.indicesLength);
		geo.drawRange.start = 0;
		geo.drawRange.count = this.indicesLength;
		geo.computeVertexNormals();
	}

	addMaterialGroup (indicesLength: number, materialGroup: number) {
		const currentGroup = this.materialGroups[this.materialGroups.length - 1];

		if (currentGroup === undefined || currentGroup[2] !== materialGroup) {
			this.materialGroups.push([this.indicesLength, indicesLength, materialGroup]);
		} else {
			currentGroup[1] += indicesLength;
		}
	}

	private closeMaterialGroups () {
		const geometry = this.geometry as THREE.BufferGeometry;
		for (let i = 0; i < this.materialGroups.length; i++) {
			const [startIndex, count, materialGroup] = this.materialGroups[i];

			geometry.addGroup(startIndex, count, materialGroup);
		}
	}

	findMaterialGroup (slotTexture: THREE.Texture, slotBlendMode: BlendMode) {
		const blendingObject = ThreeJsTexture.toThreeJsBlending(slotBlendMode);
		let group = -1;

		if (Array.isArray(this.material)) {
			for (let i = 0; i < this.material.length; i++) {
				const meshMaterial = this.material[i] as MaterialWithMap;

				if (!meshMaterial.map) {
					updateMeshMaterial(meshMaterial, slotTexture, blendingObject);
					return i;
				}

				if (meshMaterial.map === slotTexture
					&& blendingObject.blending === meshMaterial.blending
					&& (blendingObject.blendSrc === undefined || blendingObject.blendSrc === meshMaterial.blendSrc)
					&& (blendingObject.blendDst === undefined || blendingObject.blendDst === meshMaterial.blendDst)
					&& (blendingObject.blendSrcAlpha === undefined || blendingObject.blendSrcAlpha === meshMaterial.blendSrcAlpha)
					&& (blendingObject.blendDstAlpha === undefined || blendingObject.blendDstAlpha === meshMaterial.blendDstAlpha)
				) {
					return i;
				}
			}

			const meshMaterial = this.materialFactory(SkeletonMesh.DEFAULT_MATERIAL_PARAMETERS);
			// meshMaterial.premultipliedAlpha = false;
			console.log(meshMaterial);

			if (!('map' in meshMaterial)) {
				throw new Error("The material factory must return a material having the map property for the texture.");
			}


			// meshMaterial.onBeforeCompile = ( shader ) => {

			// 	if (this.twoColorTint) {
			// 		meshMaterial.defines = {
			// 			...meshMaterial.defines,
			// 			USE_SPINE_DARK_TINT: 1,
			// 		}
			// 	}

			// 	// vec4 texColor = texture2D(u_texture, v_texCoords);
			// 	// gl_FragColor.a = texColor.a * v_light.a;
			// 	// gl_FragColor.rgb = ((texColor.a - 1.0) * v_dark.a + 1.0 - texColor.rgb) * v_dark.rgb + texColor.rgb * v_light.rgb;

			// 	shader.vertexShader = "attribute vec4 darkcolor;\n" + shader.vertexShader;
			// 	shader.vertexShader = shader.vertexShader.replace(
			// 		'#include <color_pars_vertex>',
			// 		[
			// 			"#if defined( USE_COLOR_ALPHA )",
			// 			"	varying vec4 vColor;",
			// 			"	varying vec4 v_dark;",
			// 			"#endif",
			// 		].join( '\n' )
			// 	);

			// 	shader.vertexShader = shader.vertexShader.replace(
			// 		'#include <color_vertex>',
			// 		[
			// 			"#if defined( USE_COLOR_ALPHA )",
			// 			"	vColor = vec4( 1.0 );",
			// 			"	vColor *= color;",
			// 			"	v_dark = vec4( 1.0 );",
			// 			"	v_dark *= darkcolor;",
			// 			"#endif",
			// 		].join( '\n' )
			// 	);

			// 	// console.log(shader.vertexShader);



			// 	shader.fragmentShader = shader.fragmentShader.replace(
			// 		'#include <color_pars_fragment>',
			// 		[
			// 			"#if defined( USE_COLOR_ALPHA )",
			// 			"	varying vec4 vColor;",
			// 			"	varying vec4 v_dark;",
			// 			"#endif",
			// 		].join( '\n' )
			// 	);

			// 	shader.fragmentShader = shader.fragmentShader.replace(
			// 		'#include <color_fragment>',
			// 		[
			// 			"#ifdef USE_SPINE_DARK_TINT",

			// 			"	#ifdef USE_COLOR_ALPHA",
			// 			"	        diffuseColor.a *= vColor.a;",
			// 			"	        diffuseColor.rgb *= ((diffuseColor.a - 1.0) * v_dark.a + 1.0 - diffuseColor.rgb) * v_dark.rgb + diffuseColor.rgb * vColor.rgb;",
			// 			"	#endif",

			// 			"#else",
			// 			"	#ifdef USE_COLOR_ALPHA",
			// 			"	        diffuseColor *= vColor;",
			// 			"	#endif",

			// 			"#endif",
			// 		].join( '\n' )
			// 	);

			// 	// shader.fragmentShader = shader.fragmentShader.replace(
			// 	// 	'#include <premultiplied_alpha_fragment>',
			// 	// 	[
			// 	// 		"#ifdef PREMULTIPLIED_ALPHA",
			// 	// 		"        gl_FragColor.rgb *= 0.1;",
			// 	// 		"    #endif",
			// 	// 	].join( '\n' )
			// 	// );
			// 	console.log(shader.fragmentShader);

			// }


			updateMeshMaterial(meshMaterial as MaterialWithMap, slotTexture, blendingObject);
			this.material.push(meshMaterial);
			group = this.material.length - 1;
		} else {
			throw new Error("MeshBatcher.material needs to be an array for geometry groups to work");
		}

		return group;
	}
}

function updateMeshMaterial (meshMaterial: MaterialWithMap, slotTexture: THREE.Texture, blending: ThreeBlendOptions) {
	meshMaterial.map = slotTexture;
	Object.assign(meshMaterial, blending);
	meshMaterial.needsUpdate = true;
}
