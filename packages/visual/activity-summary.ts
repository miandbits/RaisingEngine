/** Current values only; never predicts a queued activity's execution state. */
export function activityPreview(
  activity: { readonly previewAttributes?: readonly { readonly attributeId:string; readonly label?:string; readonly valueFormat?:string }[] },
  attributes: readonly { readonly id:string; readonly label:string; readonly value:number }[],
): string {
  const details = (activity.previewAttributes ?? []).flatMap(entry => {
    const attribute = attributes.find(value => value.id === entry.attributeId);
    return attribute ? [`${entry.label ?? attribute.label} ${(entry.valueFormat ?? '{value}').split('{value}').join(String(attribute.value))}`] : [];
  });
  return details.length ? `현재 ${details.join(' · ')}` : '';
}
