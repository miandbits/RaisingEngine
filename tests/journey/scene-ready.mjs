/** Wait for the intended decoded scene image and its real CSS transitions. */
export async function sceneReady(page, assetId) {
  await page.waitForFunction(id => [...document.querySelectorAll('.visual-stage img')].some(image => image.src.includes(id) && image.complete && image.naturalWidth > 0), assetId);
  await page.evaluate(async () => {
    await new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
    await Promise.all(document.getAnimations().map(animation => animation.finished.catch(() => {})));
  });
}
